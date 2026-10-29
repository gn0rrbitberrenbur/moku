moku_find_pybind11()

pybind11_add_module(_core src/bindings.cpp)
target_link_libraries(_core PRIVATE moku::core)

# scikit-build-core installs this into the wheel as pymoku/_core.*
install(TARGETS _core LIBRARY DESTINATION pymoku)