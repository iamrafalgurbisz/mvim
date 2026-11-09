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
| arg      | required | desc                                                | example         |
| -------  | -------  | -------                                             | -------         |
| --file   | no       | filepath to a file you want to open in the editor   | --file text.txt |
| --debug  | no       | opens a second terminal with debug info             | --debug         |

# Controls

## Layers
| layer   | description                                             |   
| ------- | -------                                                 | 
| NORMAL  | normal mode; only moving around the document is allowed |
| VISUAL  | visual mode; allows selecting the text                  |
| INSERT  | insert mode a.k.a typing mode; allows inserting chars   |
| CMD     | allows typing in editor commands                        |

## Keymap
| key     | layers                   | description                                                |   
| ------- | -------                  | -------                                                    | 
| \<esc>  | VISUAL, INSERT, CMD      | returns to NORMAL mode                                     |
| h       | NORMAL, VISUAL           | move left                                                  |
| j       | NORMAL, VISUAL           | move down                                                  |
| k       | NORMAL, VISUAL           | move up                                                    |
| l       | NORMAL, VISUAL           | move right                                                 |
| w       | NORMAL, VISUAL           | move to the beginning of a next word                       |
| e       | NORMAL, VISUAL           | move to the end of a next word                             |
| b       | NORMAL, VISUAL           | move to the end of a previous word                         |
| o       | NORMAL                   | inserts new line below selected one and enters INSERT mode |
| i       | NORMAL                   | enters INSERT mode                                         |
| v       | NORMAL                   | enters VISUAL mode                                         |
| :       | NORMAL                   | enters CMD mode                                            |

## Editor commands
| cmd     | description                     |   
| ------- | -------                         | 
| :w      | saves file; not implemented yet |
