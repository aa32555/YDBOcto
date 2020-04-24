
========================
Administration
========================

.. contents::
   :depth: 4

Octo uses an M routine to manage the users of the database and other administrative tasks, called %ydboctoAdmin.

----------------
ydboctoAdmin
----------------

%ydboctoAdmin allows the user to add, delete and view a list of users with show.

Usage pattern:

 .. parsed-literal::
    yottadb -r %ydboctoAdmin <action> <subAction> <arguments>

* <action> is either add, delete or show.
* <subAction> refers to :code:`user` or :code:`users`.
* <arguments> would be the specific arguments passed to the command.

+++++++++++++
Add
+++++++++++++

The following is an example of adding users to the database:

 .. parsed-literal::
    yottadb -r %ydboctoAdmin add user OctoUser

This adds OctoUser as a user of the database, after verifying the password for the user.

++++++++++++++
Delete
++++++++++++++

The following is an example of deleting users from the database:

 .. parsed-literal::
    yottadb -r %ydboctoAdmin delete user OctoUser

This deletes OctoUser from the list of users in the database.

++++++++++++++
Show
++++++++++++++

The following is an example of the database showing a list of users:

 .. parsed-literal::
    yottadb -r %ydboctoAdmin show users

This shows a list of the users of the database.
