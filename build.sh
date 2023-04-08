mkdir build
cd build

conan install .. -s build_type=Debug --build missing
cmake .. -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=ON -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

cd ../
