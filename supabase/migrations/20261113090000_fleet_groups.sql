-- Fleet groups ("Ordner" in the app tab Flotte): e.g. "Firmenbus 1", "Büro" – each with a name and a colour.
-- Every member of a firm can create, rename, recolour and delete its groups and move batteries between them.

create table public.battery_groups (
  id uuid primary key default gen_random_uuid(),
  org_id uuid not null references public.orgs (id) on delete cascade,
  name text not null check (char_length(btrim(name)) between 1 and 60),
  color text not null default '#4c8dff' check (color ~ '^#[0-9a-fA-F]{6}$'),
  sort int not null default 0,
  created_at timestamptz not null default now(),
  unique (id, org_id)
);
create index battery_groups_org on public.battery_groups (org_id, sort);
alter table public.battery_groups enable row level security;
create policy "groups: Mitglieder lesen" on public.battery_groups for select to authenticated using (public.is_member(org_id));
create policy "groups: Mitglieder anlegen" on public.battery_groups for insert to authenticated with check (public.is_member(org_id));
create policy "groups: Mitglieder ändern" on public.battery_groups for update to authenticated
  using (public.is_member(org_id)) with check (public.is_member(org_id));
create policy "groups: Mitglieder löschen" on public.battery_groups for delete to authenticated using (public.is_member(org_id));
grant select, insert, update, delete on public.battery_groups to authenticated;
revoke all on public.battery_groups from anon;

-- A battery is in at most one group of its own firm; deleting a group leaves its batteries without a group.
alter table public.batteries add column group_id uuid;
alter table public.batteries add constraint batteries_group_fk foreign key (group_id, org_id)
  references public.battery_groups (id, org_id) on delete set null (group_id);
create index batteries_group on public.batteries (group_id);

-- battery_overview gets the group and when the battery was added
drop view public.battery_overview;
create view public.battery_overview with (security_invoker = on) as
select b.id, b.org_id, b.eserial, b.type_code, b.model, b.capacity_ah, b.name, b.location, b.comment, b.retired,
       lr.measured_at as last_measured_at, lr.health as last_health, lr.status as last_status, lr.pack_v as last_pack_v,
       lr.imbalance_mv as last_imbalance_mv, lr.cycles as last_cycles, lr.source as last_source,
       (select count(*) from public.readings r2 where r2.battery_id = b.id) as readings,
       b.group_id, b.created_at
from public.batteries b
left join lateral (
  select r.measured_at, r.health, r.status, r.pack_v, r.imbalance_mv, r.cycles, r.source
  from public.readings r where r.battery_id = b.id order by r.measured_at desc limit 1
) lr on true;
grant select on public.battery_overview to authenticated;
revoke select on public.battery_overview from anon;
