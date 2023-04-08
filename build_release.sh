mkdir build
cd build

conan install .. -s build_type=Release --build missing
cmake .. -DCMAKE_BUILD_TYPE=Release

cd ../
