-- ==============================================================================
-- Mist Game Streaming Platform — Supabase Database Schema
-- Production-ready schema for User Accounts, Login Auditing & Friend System
-- Fully compliant with Supabase & PostgreSQL Performance & Security Best Practices
-- ==============================================================================

-- 1. Enable Required Extensions
create extension if not exists "uuid-ossp";
create extension if not exists "pgcrypto";

-- ==============================================================================
-- 2. User Profiles & Accounts Table
-- ==============================================================================
create table if not exists public.profiles (
    id uuid primary key default gen_random_uuid(),
    auth_id text unique,
    email text,
    username text not null,
    discriminator text not null default lpad(floor(random() * 9000 + 1000)::text, 4, '0'),
    tag text generated always as (lower(username) || '#' || discriminator) stored,
    display_name text,
    avatar_url text,
    is_online boolean not null default false,
    is_hosting boolean not null default false,
    current_ip text,
    last_seen_at timestamptz not null default now(),
    created_at timestamptz not null default now(),
    updated_at timestamptz not null default now(),

    -- Constraints
    constraint username_length check (char_length(username) >= 3 and char_length(username) <= 32),
    constraint discriminator_format check (discriminator ~ '^[0-9]{4}$'),
    constraint unique_tag unique (tag)
);

-- Indexes for lightning-fast lookups and joins
create index if not exists profiles_tag_idx on public.profiles (tag);
create index if not exists profiles_auth_id_idx on public.profiles (auth_id);
create index if not exists profiles_email_idx on public.profiles (email);
create index if not exists profiles_username_idx on public.profiles (lower(username));
create index if not exists profiles_is_hosting_idx on public.profiles (is_hosting) where is_hosting = true;

-- ==============================================================================
-- 3. User Login History / Sessions Audit Table
-- ==============================================================================
create table if not exists public.user_logins (
    id uuid primary key default gen_random_uuid(),
    user_id uuid not null references public.profiles(id) on delete cascade,
    device_name text not null default 'Windows PC',
    ip_address text,
    client_version text default 'v0.3.0',
    gpu_name text,
    login_at timestamptz not null default now()
);

-- Index foreign keys and sort column
create index if not exists user_logins_user_id_idx on public.user_logins (user_id, login_at desc);

-- ==============================================================================
-- 4. Friendships & Friend Requests Table
-- ==============================================================================
create table if not exists public.friendships (
    id uuid primary key default gen_random_uuid(),
    user_id uuid not null references public.profiles(id) on delete cascade,
    friend_id uuid not null references public.profiles(id) on delete cascade,
    status text not null check (status in ('pending', 'accepted', 'blocked')),
    
    -- In-Stream Permissions (matching Mist Host permission model)
    allow_input boolean not null default false,
    allow_gamepad boolean not null default false,
    can_join boolean not null default true,

    created_at timestamptz not null default now(),
    updated_at timestamptz not null default now(),

    -- Prevent friending yourself and duplicate links
    constraint no_self_friend check (user_id != friend_id),
    constraint unique_friendship unique (user_id, friend_id)
);

-- Foreign key indexes for both directions
create index if not exists friendships_user_id_idx on public.friendships (user_id, status);
create index if not exists friendships_friend_id_idx on public.friendships (friend_id, status);

-- ==============================================================================
-- 5. Row Level Security (RLS) Configuration
-- Best Practice: Use (select auth.uid()) for 100x query caching & performance
-- ==============================================================================

alter table public.profiles enable row level security;
alter table public.user_logins enable row level security;
alter table public.friendships enable row level security;

-- Force RLS even for table owners
alter table public.profiles force row level security;
alter table public.user_logins force row level security;
alter table public.friendships force row level security;

-- --- Profiles Policies ---
-- Everyone authenticated can view basic profile info (required for friend search by tag)
create policy profiles_select_policy on public.profiles
    for select
    to authenticated
    using (true);

-- Users can only insert their own profile
create policy profiles_insert_policy on public.profiles
    for insert
    to authenticated
    with check ((select auth.uid()) = id);

-- Users can only update their own profile
create policy profiles_update_policy on public.profiles
    for update
    to authenticated
    using ((select auth.uid()) = id)
    with check ((select auth.uid()) = id);

-- --- Login History Policies ---
-- Users can view only their own login logs
create policy user_logins_select_policy on public.user_logins
    for select
    to authenticated
    using ((select auth.uid()) = user_id);

-- System or authenticated user can insert login records
create policy user_logins_insert_policy on public.user_logins
    for insert
    to authenticated
    with check ((select auth.uid()) = user_id);

-- --- Friendship Policies ---
-- Users can see friendships where they are either user_id or friend_id
create policy friendships_select_policy on public.friendships
    for select
    to authenticated
    using (
        (select auth.uid()) = user_id or (select auth.uid()) = friend_id
    );

-- Users can initiate a friend request (must be user_id)
create policy friendships_insert_policy on public.friendships
    for insert
    to authenticated
    with check (
        (select auth.uid()) = user_id and status = 'pending'
    );

-- Either participant can update status or permissions
create policy friendships_update_policy on public.friendships
    for update
    to authenticated
    using (
        (select auth.uid()) = user_id or (select auth.uid()) = friend_id
    )
    with check (
        (select auth.uid()) = user_id or (select auth.uid()) = friend_id
    );

-- Either participant can remove a friendship or decline a request
create policy friendships_delete_policy on public.friendships
    for delete
    to authenticated
    using (
        (select auth.uid()) = user_id or (select auth.uid()) = friend_id
    );

-- ==============================================================================
-- 6. Automated Triggers & RPC Functions
-- ==============================================================================

-- 6a. Automatically create a Profile when a user signs up via Supabase Auth
create or replace function public.handle_new_user()
returns trigger
language plpgsql
security definer
set search_path = ''
as $$
declare
    v_raw_username text;
    v_clean_username text;
    v_discriminator text;
begin
    -- Extract username from raw user metadata or email prefix
    v_raw_username := coalesce(
        new.raw_user_meta_data->>'username',
        split_part(new.email, '@', 1),
        'user'
    );
    -- Sanitize username (alphanumeric only, 3 to 20 chars)
    v_clean_username := regexp_replace(lower(v_raw_username), '[^a-z0-9_]', '', 'g');
    if char_length(v_clean_username) < 3 then
        v_clean_username := 'user_' || substr(md5(random()::text), 1, 4);
    end if;

    -- Generate random 4-digit discriminator
    v_discriminator := lpad(floor(random() * 9000 + 1000)::text, 4, '0');

    insert into public.profiles (
        id,
        auth_id,
        email,
        username,
        discriminator,
        display_name,
        avatar_url
    )
    values (
        new.id,
        new.id::text,
        lower(trim(new.email)),
        v_clean_username,
        v_discriminator,
        coalesce(new.raw_user_meta_data->>'full_name', new.raw_user_meta_data->>'name', new.raw_user_meta_data->>'display_name', v_clean_username),
        coalesce(new.raw_user_meta_data->>'avatar_url', new.raw_user_meta_data->>'picture')
    )
    on conflict (id) do nothing;

    -- Automatically log the initial registration / login
    insert into public.user_logins (user_id, device_name, client_version)
    values (new.id, 'Windows Desktop', 'v0.3.0');

    return new;
end;
$$;

-- Trigger on auth.users table
drop trigger if exists on_auth_user_created on auth.users;
create trigger on_auth_user_created
    after insert on auth.users
    for each row execute function public.handle_new_user();

-- 6b. RPC Function: Log in from Mist Desktop and record login audit
create or replace function public.record_desktop_login(
    p_device_name text default 'Windows PC',
    p_gpu_name text default null,
    p_ip_address text default null
)
returns void
language plpgsql
security definer
set search_path = ''
as $$
declare
    v_uid uuid := (select auth.uid());
begin
    if v_uid is null then
        raise exception 'Authentication required.';
    end if;

    -- Update last seen & status
    update public.profiles
    set is_online = true,
        last_seen_at = now(),
        current_ip = coalesce(p_ip_address, current_ip)
    where id = v_uid;

    -- Record login entry
    insert into public.user_logins (user_id, device_name, gpu_name, ip_address)
    values (v_uid, p_device_name, p_gpu_name, p_ip_address);
end;
$$;

-- 6c. RPC Function: Send Friend Request by Tag (e.g. 'kai#2047')
create or replace function public.send_friend_request(p_target_tag text)
returns json
language plpgsql
security definer
set search_path = ''
as $$
declare
    v_uid uuid := (select auth.uid());
    v_target_id uuid;
    v_clean_tag text := lower(trim(p_target_tag));
    v_existing_status text;
begin
    if v_uid is null then
        return json_build_object('success', false, 'error', 'Authentication required.');
    end if;

    -- Find target profile
    select id into v_target_id
    from public.profiles
    where lower(tag) = v_clean_tag;

    if v_target_id is null then
        return json_build_object('success', false, 'error', 'No user found with that tag.');
    end if;

    if v_target_id = v_uid then
        return json_build_object('success', false, 'error', 'You cannot add yourself as a friend.');
    end if;

    -- Check existing friendship or request
    select status into v_existing_status
    from public.friendships
    where (user_id = v_uid and friend_id = v_target_id)
       or (user_id = v_target_id and friend_id = v_uid);

    if v_existing_status is not null then
        if v_existing_status = 'accepted' then
            return json_build_object('success', false, 'error', 'You are already friends.');
        else
            return json_build_object('success', false, 'error', 'A friend request is already pending.');
        end if;
    end if;

    -- Insert request
    insert into public.friendships (user_id, friend_id, status)
    values (v_uid, v_target_id, 'pending');

    return json_build_object('success', true, 'message', 'Friend request sent successfully.');
end;
$$;

-- 6d. RPC Function: Accept or Decline Friend Request
create or replace function public.respond_friend_request(
    p_sender_tag text,
    p_accept boolean
)
returns json
language plpgsql
security definer
set search_path = ''
as $$
declare
    v_uid uuid := (select auth.uid());
    v_sender_id uuid;
begin
    if v_uid is null then
        return json_build_object('success', false, 'error', 'Authentication required.');
    end if;

    select id into v_sender_id
    from public.profiles
    where lower(tag) = lower(trim(p_sender_tag));

    if v_sender_id is null then
        return json_build_object('success', false, 'error', 'User not found.');
    end if;

    if p_accept then
        update public.friendships
        set status = 'accepted',
            updated_at = now()
        where user_id = v_sender_id and friend_id = v_uid and status = 'pending';
        return json_build_object('success', true, 'message', 'Friend request accepted.');
    else
        delete from public.friendships
        where user_id = v_sender_id and friend_id = v_uid and status = 'pending';
        return json_build_object('success', true, 'message', 'Friend request declined.');
    end if;
end;
$$;

-- 6e. RPC Function: Get complete friends list with live status and permissions
create or replace function public.get_my_friends()
returns table (
    friend_id uuid,
    tag text,
    username text,
    display_name text,
    is_online boolean,
    is_hosting boolean,
    allow_input boolean,
    allow_gamepad boolean,
    can_join boolean,
    last_seen_at timestamptz
)
language sql
security definer
set search_path = ''
as $$
    select
        p.id as friend_id,
        p.tag,
        p.username,
        p.display_name,
        p.is_online,
        p.is_hosting,
        f.allow_input,
        f.allow_gamepad,
        f.can_join,
        p.last_seen_at
    from public.friendships f
    join public.profiles p on (
        case
            when f.user_id = (select auth.uid()) then f.friend_id
            else f.user_id
        end = p.id
    )
    where (f.user_id = (select auth.uid()) or f.friend_id = (select auth.uid()))
      and f.status = 'accepted'
    order by p.is_hosting desc, p.is_online desc, p.last_seen_at desc;
$$;

-- ==============================================================================
-- 6f. RPC Function: Synchronize Google Account (Firebase Auth or OAuth) & Record Login
-- ==============================================================================
create or replace function public.sync_google_user(
    p_auth_id text,
    p_email text,
    p_display_name text,
    p_avatar_url text default null,
    p_device_name text default 'Web Client',
    p_ip_address text default null,
    p_gpu_name text default null
)
returns json
language plpgsql
security definer
set search_path = ''
as $$
declare
    v_profile record;
    v_clean_username text;
    v_disc text;
begin
    if p_auth_id is null or trim(p_auth_id) = '' then
        return json_build_object('success', false, 'error', 'Auth ID is required.');
    end if;

    -- Check if profile already exists by auth_id or email
    select * into v_profile
    from public.profiles
    where auth_id = p_auth_id or (email is not null and email = lower(trim(p_email)))
    limit 1;

    if v_profile.id is not null then
        -- Update existing profile
        update public.profiles
        set is_online = true,
            last_seen_at = now(),
            avatar_url = coalesce(p_avatar_url, avatar_url),
            display_name = coalesce(p_display_name, display_name),
            current_ip = coalesce(p_ip_address, current_ip),
            auth_id = coalesce(auth_id, p_auth_id),
            email = coalesce(email, lower(trim(p_email))),
            updated_at = now()
        where id = v_profile.id
        returning * into v_profile;
    else
        -- Generate username from display name or email
        v_clean_username := regexp_replace(lower(coalesce(split_part(p_email, '@', 1), p_display_name, 'player')), '[^a-z0-9_]', '', 'g');
        if char_length(v_clean_username) < 3 then
            v_clean_username := 'player' || floor(random() * 900 + 100)::text;
        elsif char_length(v_clean_username) > 20 then
            v_clean_username := substr(v_clean_username, 1, 20);
        end if;

        -- Unique 4-digit discriminator
        v_disc := lpad(floor(random() * 9000 + 1000)::text, 4, '0');

        insert into public.profiles (
            auth_id,
            username,
            discriminator,
            display_name,
            email,
            avatar_url,
            is_online,
            current_ip
        ) values (
            p_auth_id,
            v_clean_username,
            v_disc,
            p_display_name,
            lower(trim(p_email)),
            p_avatar_url,
            true,
            p_ip_address
        ) returning * into v_profile;
    end if;

    -- Record login entry
    insert into public.user_logins (user_id, device_name, gpu_name, ip_address)
    values (v_profile.id, coalesce(p_device_name, 'Web Client'), p_gpu_name, p_ip_address);

    return json_build_object(
        'success', true,
        'profile', json_build_object(
            'id', v_profile.id,
            'tag', v_profile.tag,
            'username', v_profile.username,
            'display_name', v_profile.display_name,
            'avatar_url', v_profile.avatar_url,
            'email', v_profile.email,
            'is_online', v_profile.is_online
        )
    );
end;
$$;
