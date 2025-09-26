#!/bin/bash

# Define the function to generate completions for the manakan command
_manakan_completions() {
    local cur prev opts
    COMPREPLY=()
    cur="${COMP_WORDS[COMP_CWORD]}"
    prev="${COMP_WORDS[COMP_CWORD-1]}"

    # Determine the config file path based on XDG_CONFIG_HOME or .config
    if [ -n "$XDG_CONFIG_HOME" ]; then
        local config_file="$XDG_CONFIG_HOME/manakan/manakan.toml"
    else
        local config_file="$HOME/.config/manakan/manakan.toml"
    fi

    # Function to parse host keys from the TOML config file
    _get_host_keys() {
        if [ -f "$config_file" ]; then
            local keys=$(grep -Eo '^\[host\..+\]' "$config_file" | sed 's/^\[host\.//; s/\]$//')
            echo $keys
        fi
    }

    # Check the previous argument to determine if we are completing for the -t option
    if [[ ${prev} == "-t" ]]; then
        COMPREPLY=($(compgen -W "$(_get_host_keys)" -- ${cur}))
        return 0
    fi

    return 0
}

# Register the completion function for the manakan command
complete -F _manakan_completions manakan
