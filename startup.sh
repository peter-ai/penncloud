#!/bin/bash

# Start a new tmux session
tmux new-session -d -s orchestrators

# Split the window into two panes horizontally
tmux split-window -h

# Split the first pane (top left) vertically
tmux select-pane -t 0
tmux split-window -v

# Split the second pane (top right) vertically
tmux select-pane -t 2
tmux split-window -v

# Run scripts in each pane
tmux send-keys -t 0 'bash ./admin_console/run_admin.sh' C-m
sleep 1
tmux send-keys -t 1 'bash ./coordinator/run_coordinator.sh' C-m
tmux send-keys -t 2 'bash ./loadbalancer/run_loadbalancer.sh' C-m
tmux send-keys -t 3 'bash ./relay/run_relay.sh' C-m

# Attach to the session
tmux attach -t orchestrators