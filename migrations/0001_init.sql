-- Users & auth
create extension if not exists citext;

create table if not exists users (
  id uuid primary key default gen_random_uuid(),
  email citext not null unique,
  password_hash text not null,
  role text not null check (role in ('user','admin')) default 'user',
  created_at timestamptz not null default now()
);

-- Saved vehicles
create table if not exists vehicles (
  id uuid primary key default gen_random_uuid(),
  user_id uuid not null references users(id) on delete cascade,
  year int not null check (year between 1900 and 2100),
  make text not null,
  model text not null,
  nickname text,
  created_at timestamptz not null default now()
);

-- Search job tracking (queue handled by Redis later)
create table if not exists search_jobs (
  id uuid primary key default gen_random_uuid(),
  user_id uuid not null references users(id) on delete cascade,
  year int not null check (year between 1900 and 2100),
  make text not null,
  model text not null,
  part text not null,
  status text not null check (status in ('queued','running','done','error')) default 'queued',
  error text,
  created_at timestamptz not null default now()
);
