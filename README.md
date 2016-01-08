# ref - a shell command log management tool

## Introdution
* ref can keep the output logs of your commands
* ref can get tokens from a log and execute a command refer to the tokens

## Compile and Install
```shell
$gcc -o ref ref.c
$cp -a ref /usr/bin
```
## Usage
Executing a command and saving the output log (both stdout and stderr) -- ref \<cmd\>
```shell
$ref grep -r main src
# output
switch head to 'ref1'
grep -r main src
    1: src/test.c:int main (int argc, char **argv)
```

Showing Command history and current head -- ref -h
```shell
$ref -h
# output
* ref1  2016-02-08 17:34 Mon - grep -r main src
  ref0  2016-02-08 17:31 Mon - git push origin master
```

Showing logs of current head -- ref -l
```shell
$ref -l
# output
grep -r main src
    1: src/test.c:int main (int argc, char **argv)
```

Showing tokens of specific row -- ref -l -\<row\>
```shell
$ref -l -1
# output
1: src/test.c:int main (int argc, char **argv)
|- 1,1: src/test.c
|- 1,2: int
|- 1,3: main
|- 1,4: int
|- 1,5: argc
|- 1,6: char
|- 1,7: **argv
```

Change default delimiter -- export REF_DEL=\<new delimiter\>
```shell
$export REF_DEL=":"
$ref -l -1
# output
1: src/ref.c:int main (int argc, char **argv)
|- 1,1: src/ref.c
|- 1,2: int main (int argc, char **argv)
```

Execute a command with specific token -- ref -e \<cmd\> -\<row\>,\<col\>
```shell
# same as $vim `ref -l -1,1`
$ref -e vim -1,1
# output
vim src/test.c
```

Execute the command of head again and update it's log -- ref -u
```shell
$ref -u
# output
grep -r main src
    1: src/test.c:int main (void)
```

Switch the head of history -- ref -t <ref#>
```shell
$ref -t ref0
# output
switch head to 'ref0'
```

Delete the records of history -- ref -d \<ref#\> ... | \<ref#\>-\<ref#\>
```shell
# same as $ref -d ref0-ref2
$ref -d ref0 ref1 ref2
# output
ref0 has been removed.
ref1 has been removed.
ref2 has been removed.
```

Others -- ref
```shell
$ref
# ouptut
usage : ref [ref#] <cmd> [-<row>[,<col>] | -d<row>,<col>]
        ref -l [ref#] [-<row>[,<col>] | -d<row>,<col>]
        ref -e [ref#] <cmd> [-<row>[,<col>] | -d<row>,<col>]
        ref -h [-all | -n <num>]
        ref -u [ref#]
        ref -t <ref#>
        ref -d <ref#> ... | <ref#>-<ref#>
```

##Q & A
1. Where are the logs saved?
> All output logs will be saved in .ref/log/ref#.

2. Can I save logs with color?
> Yes, moreover, ref will remove the ANSI escape code when you are tring to refer to a colorful token to prevent malfunctin.

3. How can I save logs with color?
> It depends on commnad. Most commands will not output colorful log when you pipe their output. However, they still provide some methods to enable that forely. ex. grep --color=always

