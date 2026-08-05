create table if not exists public.castalia_device_daily_editions (
  username text not null check (username ~ '^[a-z0-9][a-z0-9_-]{0,63}$'),
  issue_date date not null,
  source_schema text not null,
  effective_at timestamptz not null,
  payload jsonb not null,
  generated_at timestamptz not null default now(),
  updated_at timestamptz not null default now(),
  primary key (username, issue_date),
  constraint castalia_device_daily_payload_object
    check (jsonb_typeof(payload) = 'object')
);

alter table public.castalia_device_daily_editions enable row level security;

comment on table public.castalia_device_daily_editions is
  'Bounded Gazetteer payloads for paired eINQ devices; service-role only.';

revoke all on table public.castalia_device_daily_editions from anon, authenticated;
