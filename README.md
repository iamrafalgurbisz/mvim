# MVIM
MVIM stands for MyVIM. It's my VIM implementation. Cool, right?

# Quickstart
Keep in mind that the program was only tested and written for Ubuntu 26.04
```
git clone https://github.com/iamrafalgurbisz/mvim
cd mvim

clang main.c && ./a.out --file text.txt --debug
```

# Arguments
Order doesn't matter.
| arg      | required | desc    | example |
| -------- | -------  | ------- | ------- |
| --file   | no       | filepath to a file you want to open in the editor   | --file text.txt       |
| --debug  | no       | opens a second terminal with debug info   | --debug       |
