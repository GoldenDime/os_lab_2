запуск
gcc -o parallel_qsort parallel_qsort.c -lpthread

seq 1000 | shuf > input.txt

./parallel_qsort 4 < input.txt > sorted_output.txt
