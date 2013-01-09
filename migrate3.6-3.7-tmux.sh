#!/bin/sh

echo "Patching up tmux conf"
echo 'set -g default-terminal screen-256color' >> ~/.tmux.conf
echo "Patching up bash conf"
echo 'if [[ -n "$TMUX" ]]; then' >> ~/.bashrc
echo '	export TERM="screen-256color"' >> ~/.bashrc
echo 'else' >> ~/.bashrc
echo '	export TERM="xterm-256color"' >> ~/.bashrc
echo 'fi' >> ~/.bashrc
echo "All done!"
