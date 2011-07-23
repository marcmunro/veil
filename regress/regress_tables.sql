------------
-- regress_tables.sql
--
--      Table creation script for Veil regression tests.
--
--      Copyright (c) 2005 - 2011 Marc Munro
--      Author:  Marc Munro
--	License: BSD
--
------------
-- Tables for regression tests

\echo - Creating tables...
\echo - ...privileges...
create table privileges (
    privilege_id	integer not null,
    privilege_name	varchar(80) not null
);
alter table privileges add constraint privilege__pk
    primary key(privilege_id);

\echo - ...role...
create table roles (
    role_id	integer not null,
    role_name	varchar(80) not null
);
alter table roles add constraint role__pk
    primary key(role_id);

\echo - ...role_privileges...
create table role_privileges (
    role_id		integer not null,
    privilege_id	integer not null
);

alter table role_privileges add constraint role_privilege__pk
    primary key(role_id, privilege_id);

alter table role_privileges add constraint role_privilege__role_fk
    foreign key(role_id)
    references roles(role_id);

alter table role_privileges add constraint role_privilege__priv_fk
    foreign key(privilege_id)
    references privileges(privilege_id);

