-- The battery's location is taken from the first upload too (not only from later ones).
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
  insert into public.batteries (org_id, eserial, type_code, model, capacity_ah, location)
    values (p_org, v_eserial, (s ->> 'type_code')::int, s ->> 'model', (s ->> 'capacity_ah')::numeric, nullif(m ->> 'loc', ''))
    on conflict (org_id, eserial) do update
      set type_code = coalesce(excluded.type_code, public.batteries.type_code),
          model = coalesce(excluded.model, public.batteries.model),
          capacity_ah = coalesce(public.batteries.capacity_ah, excluded.capacity_ah),
          location = coalesce(excluded.location, public.batteries.location),
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
