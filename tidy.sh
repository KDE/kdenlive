#!/bin/bash


if [ -f compile_commands.json ]; then
    echo "Using the existing compilation db"
else
    echo "Generating the compile db"
    cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON .
fi

echo "Retrieving the list of files to process..."
git ls-files > file_list
touch to_process
touch headers

echo "Cleaning the list of files to process..."
for file in `cat file_list`;
do
    if grep -Fq "${file}" compile_commands.json
    then
        echo $file >> to_process
    fi
    if echo $file | grep -q "\.[ih][p]*$"
    then
        echo $file >> headers
    fi
done;

echo "Formating source files"
parallel -j 8 clang-format -i -style=file {} :::: to_process
echo "Formating header files"
parallel -j 8 clang-format -i -style=file {} :::: headers

echo "Linting"
parallel -j 8 clang-tidy -fix -config="" {} :::: to_process

echo "Reformating source files"
parallel -j 8 clang-format -i -style=file {} :::: to_process
echo "Reformating header files"
parallel -j 8 clang-format -i -style=file {} :::: headers

rm file_list
rm to_process
rm headers
