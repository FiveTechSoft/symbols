CMakeLists.txt is missing project(). Add a project(<name> C) line after cmake_minimum_required. The checker runs cmake configure and fails while cmake warns that project() is absent.
