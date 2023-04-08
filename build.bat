mkdir build
cd build

conan install .. -s build_type=Debug --build missing
cmake .. -G "Visual Studio 17" -DCMAKE_BUILD_TYPE=Debug

cd ../
