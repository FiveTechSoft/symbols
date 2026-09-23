Makefile target all does not depend on app. Fix the Makefile so `make -n all` would show a build of app (checker: runs make -n all and requires the app link line, or fails if make is unavailable).
