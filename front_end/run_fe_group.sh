#!/bin/bash

# Start a new tmux session
tmux new-session -d -s fe_group

# Split the window into two panes horizontally
tmux split-window -h

# Split the first pane (left) horizontally
tmux select-pane -t 0
tmux split-window -h

# Run scripts in each pane
tmux send-keys -t 0 'bash ./run_frontend0.sh' C-m
sleep 3
tmux send-keys -t 1 'bash ./run_frontend1.sh' C-m
tmux send-keys -t 2 'bash ./run_frontend2.sh' C-m

# Optionally, you can set the layout to even sizes for all panes
tmux select-layout even-horizontal

# Attach to the session
tmux attach -t fe_group