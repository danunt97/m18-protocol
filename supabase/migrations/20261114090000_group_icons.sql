-- Fleet groups get a symbol (van, car, office, workshop …) next to their colour.
alter table public.battery_groups add column icon text not null default 'folder'
  check (icon in ('folder', 'van', 'car', 'truck', 'trailer', 'office', 'workshop', 'warehouse', 'site', 'home',
                  'toolbox', 'charge', 'person', 'repair'));
