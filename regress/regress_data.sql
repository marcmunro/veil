------------
-- regress_data.sql
--
--      Data creation script for Veil regression tests
--
--      Copyright (c) 2005 - 2011 Marc Munro
--      Author:  Marc Munro
--	License: BSD
--
-- Data for regression tests

\echo - Populating tables...
begin;

insert into roles
       (role_id, role_name)
values (10001, 'DBA');

insert into roles
       (role_id, role_name)
values (10002, 'GRUNT');

insert into privileges
       (privilege_id, privilege_name)
values (20001, 'select_x');

insert into privileges
       (privilege_id, privilege_name)
values (20002, 'select_y');

insert into privileges
       (privilege_id, privilege_name)
values (20070, 'select_z');

insert into role_privileges
       (role_id, privilege_id)
values (10001, 20001);

insert into role_privileges
       (role_id, privilege_id)
values (10001, 20002);

insert into role_privileges
       (role_id, privilege_id)
values (10001, 20070);

insert into role_privileges
       (role_id, privilege_id)
values (10002, 20001);

commit;