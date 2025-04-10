# Cano
Cano (kay-no) is a VIM inspired modal-based text editor written in C using the [ncurses](https://opensource.apple.com/source/old_ncurses/old_ncurses-1/ncurses/test/ncurses.c.auto.html) library. 

![Cano icon](assets/cano.png) \
Icon made by [LocalTexan](https://github.com/LocalTexan)

# Demo
[![asciicast](https://asciinema.org/a/655184.svg)](https://asciinema.org/a/655184)

## Quick Start
Cano has the following dependencies:
- [GCC](https://gcc.gnu.org/)
- [ncurses](https://opensource.apple.com/source/old_ncurses/old_ncurses-1/ncurses/test/ncurses.c.auto.html)

1. Navigate to the Cano directory
```sh
cd path/to/cano
```

2. Bootstrap nobuild
```sh
cc nob.c -o nob
```

3. Build cano
```sh
./nob
```

The build files will be stored at `path/to/cano/build`.

5. Run cano
```sh
./build/cano
```

## Modes
Normal - For motions and deletion \
Insert - For inserting text \
Visual - For selecting text and performing actions on them \
Search - For searching of text in the current buffer \
Command - For executing commands

## Keybinds
|Mode  | Keybind        | Action                                          |
|------|----------------|-------------------------------------------------|
|Global| Ctrl + Q       | Quit (regardless of mode)                       |
|Global| Esc            | Enter Normal Mode                               |
|Normal| h              | Move cursor left                                |
|Normal| j              | Move cursor down                                |
|Normal| k              | Move cursor up                                  |
|Normal| l              | Move cursor right                               |
|Normal| x              | Delete character                                |
|Normal| g              | Go to first line                                |
|Normal| G              | Go to last line                                 |
|Normal| 0              | Go to beginning of line                         |
|Normal| $              | Go to end of line                               |
|Normal| w              | Go to next word                                 |
|Normal| b              | Go to last word                                 |
|Normal| e              | Go to end of next word                          |
|Normal| o              | Create line below current                       |
|Normal| O              | Create line above current                       |
|Normal| Ctrl + o       | Create line below current without changing mode |
|Normal| %              | Go to corresponding brace                       |
|Normal| i              | Enter insert mode                               |
|Normal| I              | Go to beginning of line                         |
|Normal| a              | Insert mode on next char                        |
|Normal| A              | Insert mode at end of line                      |
|Normal| v              | Enter visual mode                               |
|Normal| V              | Enter visual mode by line                       |
|Normal| u              | Undo                                            |
|Normal| U              | Redo                                            |
|Normal| /              | Enter Search mode                               |
|Normal| n              | Jump to next search                             |
|Normal| Ctrl + S       | Save and exit                                   |
|Normal| r              | Replace current char with next char inputted    |
|Normal| (n) + motion   | Repeat next motion n times                      |
|Normal| (d) + motion   | Delete characters of next motion n times        |
|Normal| Ctrl + n       | Open file explorer                              |

## Visual
Visual mode works the same as Normal mode, except it works on the entire selection, instead of character by character.
| Keybind        | Action                                          |
|----------------|-------------------------------------------------|
| >              | Indent current selection                        |
| <              | Unindent current selection                      |

## Search
Search mode takes a string and finds it in the file.
if prepended with 's/' then it will replace the first substring with the second.

Example: Using the following command
```sh
s/hello/goodbye
```
Will replace hello with goodbye.

## Commands 
| Command               | Action                                                    |
|-----------------------|-----------------------------------------------------------|
| set-output            | change output file                                        |
| echo (v)              | echo value (v) where v is either an ident or a literal    |
| we                    | Write and exit                                            |
| e                     | Write without exiting                                     |
| set-var (var) (value) | Change a config variable                                  |
| set-map (a) "(b)"     | Map key a to any combination of keys b                    |
| let (n) (v)           | Create variable (n) with value (v)                        |
| !(command)            | Execute a shell command                                   |

### Special Keys
There are a couple special keys for the key remaps.
| Key          |
|--------------|
| \<space>     |
| \<esc>       |
| \<backspace> |
| \<enter>     |
| \<ctrl-t>    |

## Config file
The config file is stored in ~/.config/cano/init.lua by default, or can be set at runtime like so:
```sh
./cano --config <config_file>
```

The `init.lua` is the entrypoint of your cano configuration.
You need to call the setup() function in order to make changes to the default configuration.
Cano will setup the following default configuration for you if you decide not to configure anything by yourself.

```lua
setup({
	syntax = true, -- toggle syntax highlighting on-off
	auto_indent = true, -- toggle auto indentation on-off
	relative = true, -- toggle relative line numbers
	indent = 0, -- set indent (value of 0 stands for 1 Tab)
	undo_size = 16, -- size of undo history
})
```

The first parameter of the setup() function must be a table.
You are free to decide which of the settings above you want to change.
The settings that you don't provide will remain with their default value.

Therefore, it is also perfectly valid to call setup() with an empty table:
```lua
setup({})
```

There is also a way to interact with cano from your configuration.
The setup() function returns a table of functions you can use to do so.
```lua
local cano = setup({
	syntax = true, -- toggle syntax highlighting on-off
	auto_indent = true, -- toggle auto indentation on-off
	relative = true, -- toggle relative line numbers
	indent = 0, -- set indent (value of 0 stands for 1 Tab)
	undo_size = 16, -- size of undo history
})

cano.exit(42, "Yoo")
```

The following functions are available:
```lua
---@param code The return code of cano on exit
---@param message This message will get printed on exit
function exit(code, message) end
```

There is a secondary config file, which is for custom syntax highlighting. It is stored in the same folder as the regular config, but uses a different naming format.
An example is ~/.config/cano/c.cyntax (spelled cyntax, with a c). The c can be replaced with whatever the file extension of your language is, such as go.cyntax for Golang.
Here is an example of a cyntax file:
```sh
k,170,68,68,
auto,struct,break,else,switch,case,
enum,register,typedef,extern,return,
union,continue,for,signed,void,do,
if,static,while,default,goto,sizeof,
volatile,const,unsigned.
t,255,165,0,
double,size_t,int,
long,char,float,short.
w,128,160,255.
```
There's a bit to unpack here, basically the single characters represent the type of the keywords:
k - Keyword
t - Type
w - Word
The type is then followed by the RGB values, all comma separated <b>without</b> spaces. After the RGB values, there is the actual keywords. End each type with a dot '.' as seen above, to indicate to Cano that the list is finished. The words are meant to be left blank, as it will highlight any words not found in the keywords above with the chosen RGB color.
If you wish to only set the color, you can provide no keywords to any, and it will fill in the keywords with C keywords by default.

## Config Variables
Config variables can also be modified at runtime by using `:ser-var ...`.
For now, the names of runtime config variables slightly differ from
the names you would specify in your `init.lua`.
This will most likely change soon.

```sh
relative # toggle relative line numbers
auto-indent # toggle auto indentation on-off
syntax # toggle syntax highlighting on-off
indent # set indent
undo-size # size of undo history 
```

# Installation

## Arch
[![Packaging status](https://repology.org/badge/vertical-allrepos/cano.svg)](https://repology.org/project/cano/versions) \
A package is provided within this [AUR](https://aur.archlinux.org/packages/cano).
You can install it using your preferred aur helper:

For instance, if using yay, do the following:
```sh
yay -S cano-git
```

## Nix / NixOS

Cano's build system recently changed, Nix installation is not supported for now.

## Debian/Ubuntu

You may build from source and install cano directly to `/usr/local/bin`. You must have a basic C compiler and the ncurses library installed (install shown below).

```sh
sudo apt install gcc libncurses-dev
git clone https://github.com/CobbCoding1/Cano && cd Cano
gcc nob.c -o nob
sudo ./nob --install
```

## Canoon (Beta)
The official Cano installer and manager, currently in Beta.
[canoon](https://github.com/kul-sudo/canoon)
