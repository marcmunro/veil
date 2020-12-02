var Demo =
[
    [ "The Veil Demo Application", "Demo.html#demo-sec", [
      [ "Installing the Veil demo", "Demo.html#demo-install", null ]
    ] ],
    [ "The Demo Database ERD, Tables and Views", "demo-model.html", [
      [ "The Demo Database ERD", "demo-model.html#demo-erd", null ],
      [ "Table Descriptions", "demo-model.html#demo-tables", [
        [ "Privileges", "demo-model.html#demo-privs", null ],
        [ "Roles", "demo-model.html#demo-roles", null ],
        [ "Role_Privileges", "demo-model.html#demo-role-privs", null ],
        [ "Role_Roles", "demo-model.html#demo-role-roles", null ],
        [ "Persons", "demo-model.html#demo-persons", null ],
        [ "Projects", "demo-model.html#demo-projects", null ],
        [ "Person_Roles", "demo-model.html#demo-person-roles", null ],
        [ "Assignments", "demo-model.html#demo-assignments", null ],
        [ "Detail_Types", "demo-model.html#demo-detail_types", null ],
        [ "Person_Details", "demo-model.html#demo-person_details", null ],
        [ "Project_Details", "demo-model.html#demo-project-details", null ]
      ] ],
      [ "The Demo Application's Helper Views", "demo-model.html#Demo-Views", null ]
    ] ],
    [ "The Demo Database Security Model", "demo-security.html", [
      [ "The Demo Database Security Model", "demo-security.html#demo-secmodel", [
        [ "The Global Context", "demo-security.html#demo-global-context", null ],
        [ "Personal Context", "demo-security.html#demo-personal-context", null ],
        [ "Project Context", "demo-security.html#demo-project-context", null ]
      ] ]
    ] ],
    [ "Exploring the Demo", "demo-explore.html", [
      [ "Exploring the Demo", "demo-explore.html#demo-use", [
        [ "Accessing the Demo Database", "demo-explore.html#demo-connect", null ]
      ] ]
    ] ],
    [ "The Demo Code", "demo-code.html", [
      [ "The Code", "demo-code.html#demo-codesec", [
        [ "veil.veil_demo_init(performing_reset bool)", "demo-code.html#demo-code-veil-init", null ],
        [ "connect_person(_person_id int4)", "demo-code.html#demo-code-connect-person", null ],
        [ "i_have_global_priv(priv_id int4)", "demo-code.html#demo-code-global-priv", null ],
        [ "i_have_personal_priv(priv_id int4, person_id int4)", "demo-code.html#demo-code-personal-priv", null ],
        [ "i_have_project_priv(priv_id int4, project_id int4)", "demo-code.html#demo-code-project-priv", null ],
        [ "i_have_proj_or_pers_priv(priv_id int4, project_id int4, person_id int4)", "demo-code.html#demo-code-proj-pers-priv", null ],
        [ "i_have_person_detail_priv(detail_id int4, person_id int4)", "demo-code.html#demo-code-pers-detail-priv", null ]
      ] ]
    ] ],
    [ "Removing The Demo Database", "demo-uninstall.html", [
      [ "Removing The Demo Database", "demo-uninstall.html#demo-clean-up", null ]
    ] ]
];