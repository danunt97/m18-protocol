-- M18 Akku-Cloud: firms, bridges, batteries, readings.
-- Every reading keeps the bridge's complete result (all raw register blocks = all 185 values,
-- trace, location, comment), so nothing measured is lost. Access: members of a firm see its data
-- (row level security); bridges upload with their own device token through bridge_ingest().

-- ------------------------------------------------------------------ tables
create table public.orgs (
  id uuid primary key default gen_random_uuid(),
  name text not null check (length(name) between 1 and 100),
  created_by uuid references auth.users (id) on delete set null,
  created_at timestamptz not null default now()
);

create table public.org_members (
  org_id uuid not null references public.orgs (id) on delete cascade,
  user_id uuid not null references auth.users (id) on delete cascade,
  role text not null default 'member' check (role in ('owner', 'member')),
  created_at timestamptz not null default now(),
  primary key (org_id, user_id)
);
create index org_members_user_idx on public.org_members (user_id);

create table public.bridges (
  id uuid primary key default gen_random_uuid(),
  org_id uuid not null references public.orgs (id) on delete cascade,
  name text not null,                       -- M18-Bridge-XXXX
  mac text,
  token_hash text not null unique,          -- sha256 of the device token (the token itself is never stored)
  firmware text,
  revoked boolean not null default false,
  last_seen timestamptz,
  created_by uuid references auth.users (id) on delete set null,
  created_at timestamptz not null default now()
);
create index bridges_org_idx on public.bridges (org_id);

create table public.batteries (
  id uuid primary key default gen_random_uuid(),
  org_id uuid not null references public.orgs (id) on delete cascade,
  eserial text not null,                    -- electronic serial "type-serial", e.g. 306-412877
  type_code int,
  model text,
  capacity_ah numeric,
  name text,
  location text,
  comment text,
  case_serial text,
  retired boolean not null default false,
  created_at timestamptz not null default now(),
  updated_at timestamptz not null default now(),
  unique (org_id, eserial)
);

create table public.readings (
  id uuid primary key default gen_random_uuid(),
  org_id uuid not null references public.orgs (id) on delete cascade,
  battery_id uuid not null references public.batteries (id) on delete cascade,
  bridge_id uuid references public.bridges (id) on delete set null,
  measured_at timestamptz not null,
  ts_source text,                           -- ntp / browser / pc / app / none ...
  source text not null,                     -- app-ble, app-wlan, bridge, pc
  location text,
  comment text,
  health int,
  status text,
  pack_v numeric,
  temp_c numeric,
  imbalance_mv int,
  cells_mv int[],
  cycles numeric,
  discharge_ah numeric,
  counters jsonb,
  warnings jsonb,
  mem jsonb not null,                       -- raw register blocks {"0000":"0005",...}: every value the pack delivered
  scan jsonb not null,                      -- complete result of the bridge (incl. trace, health parts)
  dedup_key text not null,                  -- same read-out uploaded twice (app and bridge) is stored once
  uploaded_by uuid references auth.users (id) on delete set null,
  created_at timestamptz not null default now(),
  unique (org_id, dedup_key)
);
create index readings_battery_idx on public.readings (battery_id, measured_at desc);
create index readings_org_idx on public.readings (org_id, measured_at desc);
create index readings_bridge_idx on public.readings (bridge_id);
create index batteries_org_idx on public.batteries (org_id);
create index orgs_created_by_idx on public.orgs (created_by);
create index bridges_created_by_idx on public.bridges (created_by);
create index readings_uploaded_by_idx on public.readings (uploaded_by);

-- ------------------------------------------------------------------ helpers
create or replace function public.is_member(p_org uuid)
returns boolean language sql stable security definer set search_path = '' as $$
  select exists (select 1 from public.org_members m where m.org_id = p_org and m.user_id = (select auth.uid()));
$$;

create or replace function public.is_owner(p_org uuid)
returns boolean language sql stable security definer set search_path = '' as $$
  select exists (select 1 from public.org_members m where m.org_id = p_org and m.user_id = (select auth.uid()) and m.role = 'owner');
$$;

-- New account: own firm with the user as owner.
create or replace function public.handle_new_user()
returns trigger language plpgsql security definer set search_path = '' as $$
declare v_org uuid;
begin
  insert into public.orgs (name, created_by)
    values (coalesce(nullif(new.raw_user_meta_data ->> 'org_name', ''), 'Meine Firma'), new.id)
    returning id into v_org;
  insert into public.org_members (org_id, user_id, role) values (v_org, new.id, 'owner');
  return new;
end $$;
create trigger on_auth_user_created after insert on auth.users
  for each row execute function public.handle_new_user();

-- Stores one read-out (shared by the app and the bridge upload). p_payload:
--   {"scan": {...result of the bridge...}, "meta": {"ts": unix, "ts_src": "...", "loc": "...", "comment": "..."}}
create or replace function public._store_reading(p_org uuid, p_bridge uuid, p_user uuid, p_source text, p_payload jsonb)
returns jsonb language plpgsql security definer set search_path = '' as $$
declare
  s jsonb := p_payload -> 'scan';
  m jsonb := coalesce(p_payload -> 'meta', '{}'::jsonb);
  v_eserial text := s ->> 'eserial';
  v_bat uuid;
  v_id uuid;
  v_ts timestamptz;
  v_key text;
begin
  if s is null or coalesce((s ->> 'ok')::boolean, false) is false or v_eserial is null or s -> 'mem' is null then
    return jsonb_build_object('ok', false, 'error', 'keine gültige Messung (ok, eserial und mem nötig)');
  end if;
  v_ts := case when (m ->> 'ts') ~ '^[0-9]+$' and (m ->> 'ts')::bigint > 1600000000
               then to_timestamp((m ->> 'ts')::bigint) else now() end;
  v_key := md5(v_eserial || ':' || (s -> 'mem')::text);
  insert into public.batteries (org_id, eserial, type_code, model, capacity_ah)
    values (p_org, v_eserial, (s ->> 'type_code')::int, s ->> 'model', (s ->> 'capacity_ah')::numeric)
    on conflict (org_id, eserial) do update
      set type_code = coalesce(excluded.type_code, public.batteries.type_code),
          model = coalesce(excluded.model, public.batteries.model),
          capacity_ah = coalesce(public.batteries.capacity_ah, excluded.capacity_ah),
          location = coalesce(nullif(m ->> 'loc', ''), public.batteries.location),
          updated_at = now()
    returning id into v_bat;
  insert into public.readings (org_id, battery_id, bridge_id, measured_at, ts_source, source, location, comment,
      health, status, pack_v, temp_c, imbalance_mv, cells_mv, cycles, discharge_ah, counters, warnings, mem, scan,
      dedup_key, uploaded_by)
    values (p_org, v_bat, p_bridge, v_ts, coalesce(m ->> 'ts_src', 'unbekannt'), p_source,
      nullif(m ->> 'loc', ''), nullif(m ->> 'comment', ''),
      case when (s ->> 'health')::int >= 0 then (s ->> 'health')::int end, s ->> 'status',
      (s ->> 'pack_v')::numeric, (s ->> 'temp_c')::numeric, (s ->> 'imbalance_mv')::int,
      case when jsonb_typeof(s -> 'cells_mv') = 'array'
           then array(select jsonb_array_elements_text(s -> 'cells_mv')::int) end,
      (s ->> 'cycles')::numeric, (s ->> 'discharge_ah')::numeric, s -> 'counters', s -> 'warnings',
      s -> 'mem', s, v_key, p_user)
    on conflict (org_id, dedup_key) do nothing
    returning id into v_id;
  if v_id is null then
    select r.id into v_id from public.readings r where r.org_id = p_org and r.dedup_key = v_key;
    return jsonb_build_object('ok', true, 'duplicate', true, 'reading_id', v_id, 'battery_id', v_bat);
  end if;
  return jsonb_build_object('ok', true, 'duplicate', false, 'reading_id', v_id, 'battery_id', v_bat);
end $$;
revoke all on function public._store_reading(uuid, uuid, uuid, text, jsonb) from public, anon, authenticated;

-- App (logged in): upload a read-out into one of the user's firms.
create or replace function public.app_ingest(p_org uuid, p_payload jsonb, p_source text default 'app-ble')
returns jsonb language plpgsql security definer set search_path = '' as $$
begin
  if (select auth.uid()) is null or not public.is_member(p_org) then
    return jsonb_build_object('ok', false, 'error', 'kein Zugriff auf diese Firma');
  end if;
  if p_source not in ('app-ble', 'app-wlan', 'pc') then p_source := 'app-ble'; end if;
  return public._store_reading(p_org, null, (select auth.uid()), p_source, p_payload);
end $$;

-- App (logged in): link a bridge to the firm. Returns the device token ONCE - the app sends it to the bridge.
create or replace function public.bridge_register(p_org uuid, p_name text, p_mac text default null)
returns jsonb language plpgsql security definer set search_path = '' as $$
declare v_token text := encode(extensions.gen_random_bytes(24), 'hex'); v_id uuid;
begin
  if (select auth.uid()) is null or not public.is_member(p_org) then
    return jsonb_build_object('ok', false, 'error', 'kein Zugriff auf diese Firma');
  end if;
  update public.bridges set revoked = true where org_id = p_org and mac is not null and mac = p_mac and not revoked;
  insert into public.bridges (org_id, name, mac, token_hash, created_by)
    values (p_org, left(coalesce(nullif(p_name, ''), 'M18-Brücke'), 60), p_mac,
            encode(extensions.digest(v_token, 'sha256'), 'hex'), (select auth.uid()))
    returning id into v_id;
  return jsonb_build_object('ok', true, 'bridge_id', v_id, 'token', v_token);
end $$;

-- Bridge (no user login): upload with its device token. Callable with the public key.
create or replace function public.bridge_ingest(p_token text, p_payload jsonb, p_firmware text default null)
returns jsonb language plpgsql security definer set search_path = '' as $$
declare b public.bridges%rowtype;
begin
  select * into b from public.bridges
    where token_hash = encode(extensions.digest(coalesce(p_token, ''), 'sha256'), 'hex') and not revoked;
  if not found then
    return jsonb_build_object('ok', false, 'error', 'Gerät nicht verknüpft oder gesperrt');
  end if;
  update public.bridges set last_seen = now(), firmware = coalesce(p_firmware, firmware) where id = b.id;
  return public._store_reading(b.org_id, b.id, null, 'bridge', p_payload);
end $$;

grant execute on function public.app_ingest(uuid, jsonb, text) to authenticated;
grant execute on function public.bridge_register(uuid, text, text) to authenticated;
grant execute on function public.bridge_ingest(text, jsonb, text) to anon, authenticated;
revoke execute on function public.app_ingest(uuid, jsonb, text) from anon;
revoke execute on function public.bridge_register(uuid, text, text) from anon;
revoke execute on function public.handle_new_user() from public, anon, authenticated;

-- ------------------------------------------------------------------ row level security
alter table public.orgs enable row level security;
alter table public.org_members enable row level security;
alter table public.bridges enable row level security;
alter table public.batteries enable row level security;
alter table public.readings enable row level security;

create policy "orgs: Mitglieder lesen" on public.orgs for select to authenticated using (public.is_member(id));
create policy "orgs: Inhaber ändern" on public.orgs for update to authenticated using (public.is_owner(id)) with check (public.is_owner(id));

create policy "members: Mitglieder lesen" on public.org_members for select to authenticated using (public.is_member(org_id));
create policy "members: Inhaber verwalten" on public.org_members for delete to authenticated
  using (public.is_owner(org_id) or user_id = (select auth.uid()));

create policy "bridges: Mitglieder lesen" on public.bridges for select to authenticated using (public.is_member(org_id));
create policy "bridges: Mitglieder ändern" on public.bridges for update to authenticated
  using (public.is_member(org_id)) with check (public.is_member(org_id));
create policy "bridges: Inhaber löschen" on public.bridges for delete to authenticated using (public.is_owner(org_id));

create policy "batteries: Mitglieder lesen" on public.batteries for select to authenticated using (public.is_member(org_id));
create policy "batteries: Mitglieder anlegen" on public.batteries for insert to authenticated with check (public.is_member(org_id));
create policy "batteries: Mitglieder ändern" on public.batteries for update to authenticated
  using (public.is_member(org_id)) with check (public.is_member(org_id));
create policy "batteries: Inhaber löschen" on public.batteries for delete to authenticated using (public.is_owner(org_id));

create policy "readings: Mitglieder lesen" on public.readings for select to authenticated using (public.is_member(org_id));
create policy "readings: Mitglieder ändern" on public.readings for update to authenticated
  using (public.is_member(org_id)) with check (public.is_member(org_id));
create policy "readings: Mitglieder löschen" on public.readings for delete to authenticated using (public.is_member(org_id));
-- readings are inserted only through app_ingest / bridge_ingest (one place that checks and de-duplicates)

-- token_hash stays on the server: no table-wide SELECT on bridges, only these columns
revoke select on public.bridges from anon, authenticated;
grant select (id, org_id, name, mac, firmware, revoked, last_seen, created_by, created_at) on public.bridges to authenticated;
