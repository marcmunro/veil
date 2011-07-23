
\echo Creating tables...

\echo ...privileges...
create table privileges (
    privilege_id	integer not null,
    privilege_name	varchar(80) not null
);
alter table privileges add constraint privilege__pk
    primary key(privilege_id);


\echo ...role...
create table roles (
    role_id	integer not null,
    role_name	varchar(80) not null
);
alter table roles add constraint role__pk
    primary key(role_id);


\echo ...role_privileges...
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

\echo ...role_roles...
create table role_roles (
    role_id		integer not null,
    has_role_id		integer not null
);

alter table role_roles add constraint role_role__pk
    primary key(role_id, has_role_id);

alter table role_roles add constraint role_role__role_fk
    foreign key(role_id)
    references roles(role_id);

alter table role_roles add constraint role_role__has_role_fk
    foreign key(has_role_id)
    references roles(role_id);


\echo ...persons...
create sequence person_id_seq;
create table persons (
    person_id		integer not null,
    person_name		varchar(80) not null
);
alter table persons add constraint person__pk
    primary key(person_id);


\echo ...projects...
create sequence project_id_seq;
create table projects (
    project_id		integer not null,
    project_name	varchar(80) not null
);
alter table projects add constraint project__pk
    primary key(project_id);


\echo ...detail_types...
create table detail_types (
    detail_type_id	  integer not null,
    required_privilege_id integer not null,
    detail_type_name	  varchar(80) not null
);
alter table detail_types add constraint detail_type__pk
    primary key(detail_type_id);

alter table detail_types add constraint detail_type__priv_fk
    foreign key(required_privilege_id)
    references privileges(privilege_id);


\echo ...assignments...
create table assignments (
    project_id		integer not null,
    person_id		integer not null,
    role_id		integer not null
);
alter table assignments add constraint assignment__pk
    primary key(project_id, person_id);

alter table assignments add constraint assignment__project_fk
    foreign key(project_id)
    references projects(project_id);

alter table assignments add constraint assignment__person_fk
    foreign key(person_id)
    references persons(person_id);

alter table assignments add constraint assignment__role_fk
    foreign key(role_id)
    references roles(role_id);


\echo ...person_roles...
create table person_roles (
    person_id		integer not null,
    role_id		integer not null
);
alter table person_roles add constraint person_role__pk
    primary key(person_id, role_id);

alter table person_roles add constraint person_role__person_fk
    foreign key(person_id)
    references persons(person_id);

alter table person_roles add constraint person_role__role_fk
    foreign key(role_id)
    references roles(role_id);


\echo ...project_details...
create table project_details (
    project_id		integer not null,
    detail_type_id	integer not null,
    value		text not null
);
alter table project_details add constraint project_detail__pk
    primary key(project_id, detail_type_id);

alter table project_details add constraint project_detail__project_fk
    foreign key(project_id)
    references projects(project_id);

alter table project_details add constraint project_detail__detail_fk
    foreign key(detail_type_id)
    references detail_types(detail_type_id);


\echo ...person_details...
create table person_details (
    person_id		integer not null,
    detail_type_id	integer not null,
    value		text not null
);
alter table person_details add constraint person_detail__pk
    primary key(person_id, detail_type_id);

alter table person_details add constraint person_detail__person_fk
    foreign key(person_id)
    references persons(person_id);

alter table person_details add constraint person_detail__detail_fk
    foreign key(detail_type_id)
    references detail_types(detail_type_id);

\echo Setting up base data...

\echo ...privileges...
copy privileges (privilege_id, privilege_name) from stdin;
10001	select_privileges
10002	insert_privileges
10003	update_privileges
10004	delete_privileges
10005	select_roles
10006	insert_roles
10007	update_roles
10008	delete_roles
10009	select_role_privileges
10010	insert_role_privileges
10011	update_role_privileges
10012	delete_role_privileges
10013	select_persons
10014	insert_persons
10015	update_persons
10016	delete_persons
10017	select_projects
10018	insert_projects
10019	update_projects
10020	delete_projects
10021	select_detail_types
10022	insert_detail_types
10023	update_detail_types
10024	delete_detail_types
10025	select_assignments
10026	insert_assignments
10027	update_assignments
10028	delete_assignments
10029	select_person_roles
10030	insert_person_roles
10031	update_person_roles
10032	delete_person_roles
10033	select_project_details
10034	insert_project_details
10035	update_project_details
10036	delete_project_details
10037	select_person_details
10038	insert_person_details
10039	update_person_details
10040	delete_person_details
10041	select_role_roles
10042	insert_role_roles
10043	update_role_roles
10044	delete_role_roles
10100	can_connect
10150	view_basic
10151	view_personal
10152	view_personal_secure
10153	view_project_confidential
\.

\echo ...roles...
copy roles (role_id, role_name) from stdin;
11001	DBA
11002	Personal Context
11003	Employee
11004	Worker
11005	Project Manager
11006	Director
11007	Manager
\.

\echo ...role_privileges...
-- DBA can do anything (but is not automatically an employee)
insert into role_privileges (role_id, privilege_id)
select 11001, privilege_id
from   privileges
where  privilege_id != 10100;

-- Personal Context allows update of personal details
copy role_privileges (role_id, privilege_id) from stdin;
11002	10013
11002	10015
11002	10025
11002	10029
11002	10037
11002	10038
11002	10039
11002	10040
11002	10150
11002	10151
11002	10152
\.

-- Basic Access can see lookup data
insert into role_privileges (role_id, privilege_id)
select 11003, privilege_id
from   privileges
where  privilege_name in ('select_privileges', 'select_roles',
			  'select_role_privileges', 'select_detail_types');

insert into role_privileges (role_id, privilege_id)
select 11003, 10100;

-- Workers can modify project info
insert into role_privileges (role_id, privilege_id)
select 11004, privilege_id
from   privileges
where  privilege_name like '%project%'
and    privilege_name not like 'delete%'
and    privilege_name not like '%confidential';

insert into role_privileges (role_id, privilege_id)
select 11004, 10025;
insert into role_privileges (role_id, privilege_id)
select 11004, 10150;

-- Project Manager can do anything to project info and can see personal info
insert into role_privileges (role_id, privilege_id)
select 11005, privilege_id
from   privileges
where  privilege_name like '%project%'
or     privilege_name like '%assignment%';

insert into role_privileges (role_id, privilege_id)
select 11005, privilege_id
from   privileges
where  privilege_name like 'select_person%';

insert into role_privileges (role_id, privilege_id)
select 11005, 10150;
insert into role_privileges (role_id, privilege_id)
select 11005, 10151;

-- Director can do anything except modify personal details
insert into role_privileges (role_id, privilege_id)
select 11006, privilege_id
from   privileges
where  privilege_name not like '%person%';

insert into role_privileges (role_id, privilege_id)
select 11006, privilege_id
from   privileges
where  privilege_name like 'select_person%';

insert into role_privileges (role_id, privilege_id)
select 11006, 10014;

insert into role_privileges (role_id, privilege_id)
select 11006, 10151;

insert into role_privileges (role_id, privilege_id)
select 11006, 10152;

-- Manager can see personal info
insert into role_privileges (role_id, privilege_id)
select 11007, privilege_id
from   privileges
where  privilege_name like 'select_person%';

insert into role_privileges (role_id, privilege_id)
select 11007, 10150;
insert into role_privileges (role_id, privilege_id)
select 11007, 10151;


\echo ...persons...
copy persons (person_id, person_name) from stdin;
1	Deb (the DBA)
2	Pat (the PM)
3	Derick (the director)
4	Will (the worker)
5	Wilma (the worker)
6	Fred (the fired DBA)
\.

\echo ...person_roles...
copy person_roles (person_id, role_id) from stdin;
1	11001
1	11003
2	11003
2	11007
3	11003
3	11006
4	11003
5	11003
6	11001
\.

\echo ...projects...
copy projects (project_id, project_name) from stdin;
101	Secret Project
102	Public project
\.

\echo ...assignments...
copy assignments (project_id, person_id, role_id) from stdin;
101	3	11005
101	5	11004
102	2	11005
102	4	11004
102	5	11004
\.

\echo ...detail_types...
copy detail_types (detail_type_id, required_privilege_id, 
                   detail_type_name) from stdin;
1001	10150	start_date
1002	10150	status
1003	10150	join_date
1004	10152	salary
1005	10151	date of birth
1006	10152	sin
1007	10150	skills
1008	10153	contract value
\.

\echo ...person_details...
copy person_details (person_id, detail_type_id, value) from stdin;
1	1003	20050102
2	1003	20050103
3	1003	20050104
4	1003	20050105
5	1003	20050106
6	1003	20050107
1	1002	Employee
2	1002	Employee
3	1002	Employee
4	1002	Employee
5	1002	Employee
6	1002	Terminated
1	1004	50,000
2	1004	50,000
3	1004	80,000
4	1004	30,000
5	1004	30,000
6	1004	40,000
1	1005	19610102
2	1005	19600102
3	1005	19650102
4	1005	19660102
5	1005	19670102
1	1006	123456789
2	1006	123456789
3	1006	123456789
4	1006	123456789
5	1006	123456789
1	1007	Oracle, C, SQL
2	1007	Soft peoply-stuff
3	1007	None at all
4	1007	Subservience
5	1007	Subservience
\.


\echo ...project_details
copy project_details (project_id, detail_type_id, value) from stdin;
101	1001	20050101
101	1002	Secretly ongoing
101	1008	$800,000
102	1001	20050101
102	1002	Ongoing
102	1008	$100,000
\.

