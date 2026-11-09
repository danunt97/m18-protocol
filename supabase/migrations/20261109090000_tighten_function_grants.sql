-- Functions are executable by PUBLIC by default: only the intended roles may call them.
revoke execute on function public.app_ingest(uuid, jsonb, text) from public, anon;
revoke execute on function public.bridge_register(uuid, text, text) from public, anon;
revoke execute on function public.bridge_ingest(text, jsonb, text) from public;
revoke execute on function public.is_member(uuid) from public, anon;
revoke execute on function public.is_owner(uuid) from public, anon;
grant execute on function public.app_ingest(uuid, jsonb, text) to authenticated;
grant execute on function public.bridge_register(uuid, text, text) to authenticated;
grant execute on function public.bridge_ingest(text, jsonb, text) to anon, authenticated;
grant execute on function public.is_member(uuid) to authenticated;
grant execute on function public.is_owner(uuid) to authenticated;
