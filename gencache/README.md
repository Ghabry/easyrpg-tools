GENCACHE
========

GENCACHE is a small tool to generate a file listing cache used by the
EasyRPG Player on some platforms.

GENCACHE is part of the EasyRPG Project.
More information is available at the project website:

https://easy-rpg.org/


Documentation
-------------

Documentation is available at the documentation wiki:

https://easy-rpg.org/wiki/


Requirements
------------

 * ICU


Daily builds
------------

Up to date binaries for assorted platforms are available at:

https://easy-rpg.org/jenkins/


Source code
-----------

GENCACHE development is hosted by GitHub, project files are available in a git
repository.

https://github.com/EasyRPG/Tools


Building
--------

GENCACHE uses Autotools:

    ./bootstrap (only needed if using a git checkout)
    ./configure
    make
    make install (optionally)

You may tweak build parameters and environment variables, run
`./configure --help` for reference.


License
-------

GENCACHE is free software under the MIT license. See the file COPYING for
details.

It uses the "JSON for Modern C++" header made by Niels Lohmann,
available also under MIT license at: https://github.com/nlohmann/json
