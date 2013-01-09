#!/bin/bash
echo "# necessary for 256 color support" | cat >> $HOME/.screenrc
echo "attrcolor b \".I\"" | cat >> $HOME/.screenrc
echo "termcapinfo xterm 'Co#256:AB=\E[48;5;%dm:AF=\E[38;5;%dm'" | cat >> $HOME/.screenrc
echo "defbce \"on\"" | cat >> $HOME/.screenrc

echo "export TERM=xterm-256color" | cat >> $HOME/.bashrc
