-- "Meine Akkus": every battery of the user's firms with its latest reading (respects row level security).
create or replace view public.battery_overview with (security_invoker = on) as
select b.id, b.org_id, b.eserial, b.type_code, b.model, b.capacity_ah, b.name, b.location, b.comment, b.retired,
       lr.measured_at as last_measured_at, lr.health as last_health, lr.status as last_status, lr.pack_v as last_pack_v,
       lr.imbalance_mv as last_imbalance_mv, lr.cycles as last_cycles, lr.source as last_source,
       (select count(*) from public.readings r2 where r2.battery_id = b.id) as readings
from public.batteries b
left join lateral (
  select r.measured_at, r.health, r.status, r.pack_v, r.imbalance_mv, r.cycles, r.source
  from public.readings r where r.battery_id = b.id order by r.measured_at desc limit 1
) lr on true;
grant select on public.battery_overview to authenticated;
revoke select on public.battery_overview from anon;

-- Delete my account: firms where I am the only member go with it (batteries, readings, bridges);
-- in shared firms only my membership is removed.
create or replace function public.delete_my_account()
returns jsonb language plpgsql security definer set search_path = '' as $$
declare v_uid uuid := (select auth.uid()); n int;
begin
  if v_uid is null then return jsonb_build_object('ok', false, 'error', 'nicht angemeldet'); end if;
  delete from public.orgs o
    where exists (select 1 from public.org_members m where m.org_id = o.id and m.user_id = v_uid)
      and not exists (select 1 from public.org_members m where m.org_id = o.id and m.user_id <> v_uid);
  get diagnostics n = row_count;
  delete from auth.users where id = v_uid;
  return jsonb_build_object('ok', true, 'firms_deleted', n);
end $$;
revoke execute on function public.delete_my_account() from public, anon;
grant execute on function public.delete_my_account() to authenticated;
