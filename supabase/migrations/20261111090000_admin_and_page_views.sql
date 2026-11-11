-- Admin page (akkuscan.app/admin/) and anonymous page-view statistics.
--
-- Admins are listed in public.admins (nobody can read or write that table over the API).
-- Every admin_* function is SECURITY DEFINER, checks is_admin() first and returns JSON.
-- Add an admin in the SQL editor:  insert into public.admins (user_id) select id from auth.users where email = '…';

create table public.admins (
  user_id uuid primary key references auth.users (id) on delete cascade,
  created_at timestamptz not null default now()
);
alter table public.admins enable row level security;
revoke all on public.admins from anon, authenticated;

create or replace function public.is_admin()
returns boolean language sql stable security definer set search_path = '' as $$
  select exists (select 1 from public.admins where user_id = (select auth.uid()))
$$;
revoke execute on function public.is_admin() from public, anon;
grant execute on function public.is_admin() to authenticated;

-- ------------------------------------------------------------------ page views
-- No cookies, no IP address, no visitor ID: one row per page load with what the browser tells anyway.
-- "entry" = the visitor came from outside akkuscan.app (start of a visit).
create table public.page_views (
  id bigint generated always as identity primary key,
  at timestamptz not null default now(),
  path text not null,
  referrer text,
  device text,
  browser text,
  lang text,
  utm text,
  entry boolean not null default false
);
create index page_views_at on public.page_views (at desc);
alter table public.page_views enable row level security;
revoke all on public.page_views from anon, authenticated;

create or replace function public.track_view(p_path text, p_referrer text default null, p_device text default null,
  p_browser text default null, p_lang text default null, p_utm text default null, p_entry boolean default false)
returns void language plpgsql security definer set search_path = '' as $$
begin
  if p_path is null or p_path !~ '^/' then return; end if;
  insert into public.page_views (path, referrer, device, browser, lang, utm, entry)
  values (left(split_part(split_part(p_path, '?', 1), '#', 1), 120),
          nullif(left(lower(p_referrer), 80), ''),
          case when p_device in ('mobile', 'tablet', 'desktop') then p_device end,
          nullif(left(p_browser, 30), ''),
          nullif(left(p_lang, 10), ''),
          nullif(left(p_utm, 60), ''),
          coalesce(p_entry, false));
end $$;
revoke execute on function public.track_view(text, text, text, text, text, text, boolean) from public;
grant execute on function public.track_view(text, text, text, text, text, text, boolean) to anon, authenticated;

-- ------------------------------------------------------------------ admin: guard
create or replace function public._admin_guard()
returns void language plpgsql stable security definer set search_path = '' as $$
begin
  if not public.is_admin() then raise exception 'Kein Admin-Zugang' using errcode = '42501'; end if;
end $$;
revoke execute on function public._admin_guard() from public, anon, authenticated;

-- ------------------------------------------------------------------ admin: overview
create or replace function public.admin_overview(p_days int default 30)
returns jsonb language plpgsql stable security definer set search_path = '' as $$
declare d int := least(greatest(coalesce(p_days, 30), 7), 365); since timestamptz; r jsonb;
begin
  perform public._admin_guard();
  since := date_trunc('day', now()) - make_interval(days => d - 1);
  select jsonb_build_object(
    'days', d,
    'totals', jsonb_build_object(
      'users', (select count(*) from auth.users),
      'users_confirmed', (select count(*) from auth.users where email_confirmed_at is not null),
      'users_new_7d', (select count(*) from auth.users where created_at > now() - interval '7 days'),
      'users_active_7d', (select count(*) from auth.users where last_sign_in_at > now() - interval '7 days'),
      'orgs', (select count(*) from public.orgs),
      'batteries', (select count(*) from public.batteries),
      'readings', (select count(*) from public.readings),
      'readings_7d', (select count(*) from public.readings where created_at > now() - interval '7 days'),
      'bridges', (select count(*) from public.bridges where not revoked),
      'bridges_seen_7d', (select count(*) from public.bridges where not revoked and last_seen > now() - interval '7 days'),
      'views_today', (select count(*) from public.page_views where at >= date_trunc('day', now())),
      'views_7d', (select count(*) from public.page_views where at > now() - interval '7 days'),
      'visits_7d', (select count(*) from public.page_views where entry and at > now() - interval '7 days')),
    'daily', (select jsonb_agg(jsonb_build_object(
        'day', to_char(g, 'YYYY-MM-DD'),
        'signups', (select count(*) from auth.users u where u.created_at >= g and u.created_at < g + interval '1 day'),
        'readings', (select count(*) from public.readings x where x.created_at >= g and x.created_at < g + interval '1 day'),
        'views', (select count(*) from public.page_views v where v.at >= g and v.at < g + interval '1 day'),
        'visits', (select count(*) from public.page_views v where v.entry and v.at >= g and v.at < g + interval '1 day')) order by g)
      from generate_series(since, date_trunc('day', now()), interval '1 day') g),
    'sources', coalesce((select jsonb_object_agg(source, n) from (select source, count(*) n from public.readings group by source) s), '{}'),
    'status', coalesce((select jsonb_object_agg(coalesce(last_status, 'unknown'), n)
        from (select last_status, count(*) n from public.battery_overview group by last_status) s), '{}'),
    'models', coalesce((select jsonb_agg(jsonb_build_object('model', model, 'n', n) order by n desc)
        from (select coalesce(model, 'unbekannt') model, count(*) n from public.batteries group by 1 order by 2 desc limit 12) s), '[]'),
    'firmware', coalesce((select jsonb_agg(jsonb_build_object('firmware', fw, 'n', n) order by n desc)
        from (select coalesce(firmware, '–') fw, count(*) n from public.bridges where not revoked group by 1) s), '[]'),
    'top_paths', coalesce((select jsonb_agg(jsonb_build_object('path', path, 'n', n) order by n desc)
        from (select path, count(*) n from public.page_views where at >= since group by 1 order by 2 desc limit 15) s), '[]'),
    'top_referrers', coalesce((select jsonb_agg(jsonb_build_object('referrer', referrer, 'n', n) order by n desc)
        from (select referrer, count(*) n from public.page_views where at >= since and entry and referrer is not null group by 1 order by 2 desc limit 15) s), '[]'),
    'devices', coalesce((select jsonb_object_agg(coalesce(device, 'unbekannt'), n)
        from (select device, count(*) n from public.page_views where at >= since group by 1) s), '{}'),
    'browsers', coalesce((select jsonb_agg(jsonb_build_object('browser', browser, 'n', n) order by n desc)
        from (select coalesce(browser, 'unbekannt') browser, count(*) n from public.page_views where at >= since group by 1 order by 2 desc limit 10) s), '[]')
  ) into r;
  return r;
end $$;

-- ------------------------------------------------------------------ admin: accounts
create or replace function public.admin_users()
returns jsonb language plpgsql stable security definer set search_path = '' as $$
begin
  perform public._admin_guard();
  return coalesce((select jsonb_agg(jsonb_build_object(
      'id', u.id, 'email', u.email, 'created_at', u.created_at, 'confirmed_at', u.email_confirmed_at,
      'last_sign_in_at', u.last_sign_in_at,
      'admin', exists (select 1 from public.admins a where a.user_id = u.id),
      'orgs', coalesce((select jsonb_agg(jsonb_build_object('id', o.id, 'name', o.name, 'role', m.role, 'members',
                  (select count(*) from public.org_members m2 where m2.org_id = o.id)))
                from public.org_members m join public.orgs o on o.id = m.org_id where m.user_id = u.id), '[]'),
      'batteries', (select count(*) from public.batteries b where b.org_id in (select org_id from public.org_members where user_id = u.id)),
      'readings', (select count(*) from public.readings x where x.org_id in (select org_id from public.org_members where user_id = u.id)),
      'last_reading_at', (select max(x.created_at) from public.readings x where x.org_id in (select org_id from public.org_members where user_id = u.id)),
      'bridges', (select count(*) from public.bridges br where not br.revoked and br.org_id in (select org_id from public.org_members where user_id = u.id))
    ) order by u.created_at desc) from auth.users u), '[]');
end $$;

-- ------------------------------------------------------------------ admin: read-outs
create or replace function public.admin_readings(p_limit int default 200, p_offset int default 0, p_org uuid default null, p_battery uuid default null)
returns jsonb language plpgsql stable security definer set search_path = '' as $$
begin
  perform public._admin_guard();
  return coalesce((select jsonb_agg(row_to_json(t)::jsonb order by t.created_at desc) from (
    select x.id, x.created_at, x.measured_at, x.source, x.health, x.status, x.pack_v, x.imbalance_mv, x.cycles,
           x.location, x.comment, o.name as org, x.org_id, b.id as battery_id, b.eserial, b.model, b.name as battery_name,
           br.name as bridge, (select email from auth.users u where u.id = x.uploaded_by) as uploaded_by
    from public.readings x
    join public.orgs o on o.id = x.org_id
    join public.batteries b on b.id = x.battery_id
    left join public.bridges br on br.id = x.bridge_id
    where (p_org is null or x.org_id = p_org) and (p_battery is null or x.battery_id = p_battery)
    order by x.created_at desc
    limit least(greatest(coalesce(p_limit, 200), 1), 1000) offset greatest(coalesce(p_offset, 0), 0)) t), '[]');
end $$;

create or replace function public.admin_reading(p_id uuid)
returns jsonb language plpgsql stable security definer set search_path = '' as $$
begin
  perform public._admin_guard();
  return (select jsonb_build_object('id', x.id, 'created_at', x.created_at, 'measured_at', x.measured_at, 'source', x.source,
      'location', x.location, 'comment', x.comment, 'org', o.name, 'scan', x.scan)
    from public.readings x join public.orgs o on o.id = x.org_id where x.id = p_id);
end $$;

-- ------------------------------------------------------------------ admin: batteries and modules
create or replace function public.admin_batteries()
returns jsonb language plpgsql stable security definer set search_path = '' as $$
begin
  perform public._admin_guard();
  return coalesce((select jsonb_agg(jsonb_build_object(
      'id', b.id, 'org', o.name, 'org_id', b.org_id, 'eserial', b.eserial, 'model', b.model, 'name', b.name, 'location', b.location,
      'retired', b.retired, 'last_measured_at', b.last_measured_at, 'last_health', b.last_health,
      'last_status', b.last_status, 'last_cycles', b.last_cycles, 'readings', b.readings) order by b.last_measured_at desc nulls last)
    from public.battery_overview b join public.orgs o on o.id = b.org_id), '[]');
end $$;

create or replace function public.admin_bridges()
returns jsonb language plpgsql stable security definer set search_path = '' as $$
begin
  perform public._admin_guard();
  return coalesce((select jsonb_agg(jsonb_build_object(
      'id', br.id, 'name', br.name, 'mac', br.mac, 'firmware', br.firmware, 'revoked', br.revoked, 'last_seen', br.last_seen,
      'created_at', br.created_at, 'org', o.name, 'org_id', br.org_id,
      'created_by', (select email from auth.users u where u.id = br.created_by),
      'readings', (select count(*) from public.readings x where x.bridge_id = br.id)) order by br.last_seen desc nulls last)
    from public.bridges br join public.orgs o on o.id = br.org_id), '[]');
end $$;

-- ------------------------------------------------------------------ admin: visits
create or replace function public.admin_page_views(p_limit int default 300)
returns jsonb language plpgsql stable security definer set search_path = '' as $$
begin
  perform public._admin_guard();
  return coalesce((select jsonb_agg(row_to_json(t)::jsonb order by t.at desc) from (
    select at, path, referrer, device, browser, lang, utm, entry from public.page_views
    order by at desc limit least(greatest(coalesce(p_limit, 300), 1), 2000)) t), '[]');
end $$;

-- ------------------------------------------------------------------ admin: actions
-- Delete an account like delete_my_account does: firms where it is the only member go with it.
create or replace function public.admin_delete_user(p_user uuid)
returns jsonb language plpgsql security definer set search_path = '' as $$
declare n int;
begin
  perform public._admin_guard();
  if p_user = (select auth.uid()) then return jsonb_build_object('ok', false, 'error', 'Das eigene Konto kann hier nicht gelöscht werden'); end if;
  delete from public.orgs o
    where exists (select 1 from public.org_members m where m.org_id = o.id and m.user_id = p_user)
      and not exists (select 1 from public.org_members m where m.org_id = o.id and m.user_id <> p_user);
  get diagnostics n = row_count;
  delete from auth.users where id = p_user;
  return jsonb_build_object('ok', true, 'firms_deleted', n);
end $$;

-- Block / unblock a module: a revoked module can no longer upload.
create or replace function public.admin_set_bridge_revoked(p_bridge uuid, p_revoked boolean)
returns jsonb language plpgsql security definer set search_path = '' as $$
begin
  perform public._admin_guard();
  update public.bridges set revoked = coalesce(p_revoked, true) where id = p_bridge;
  return jsonb_build_object('ok', found);
end $$;

do $$
declare f text;
begin
  foreach f in array array[
    'admin_overview(int)', 'admin_users()', 'admin_readings(int, int, uuid, uuid)', 'admin_reading(uuid)',
    'admin_batteries()', 'admin_bridges()', 'admin_page_views(int)', 'admin_delete_user(uuid)',
    'admin_set_bridge_revoked(uuid, boolean)']
  loop
    execute format('revoke execute on function public.%s from public, anon', f);
    execute format('grant execute on function public.%s to authenticated', f);
  end loop;
end $$;
