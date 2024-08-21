#!/bin/bash

# Start a new tmux session
tmux new-session -d -s be_cluster_G1

# Split the window into two panes horizontally
tmux split-window -h

# Split the first pane (top left) vertically
tmux select-pane -t 0
tmux split-window -v

# Split the second pane (top right) vertically
tmux select-pane -t 2
tmux split-window -v

# Run scripts in each pane
tmux send-keys -t 0 'bash ./run_backend_G1_P.sh' C-m
sleep 5
tmux send-keys -t 1 'bash ./run_backend_G1_S1.sh' C-m
tmux send-keys -t 2 'bash ./run_backend_G1_S2.sh' C-m
tmux send-keys -t 3 'bash ./run_backend_G1_S3.sh' C-m

# # Optionally, you can set the layout to even sizes for all panes
# tmux select-layout even-horizontal

# Attach to the session
tmux attach -t be_cluster_G1