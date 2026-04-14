#!/bin/bash

_manakan_config_root() {
    if [[ -n "${XDG_CONFIG_HOME:-}" ]]; then
        printf '%s\n' "$XDG_CONFIG_HOME/manakan"
    else
        printf '%s\n' "$HOME/.config/manakan"
    fi
}

_manakan_providers_dir() {
    printf '%s/providers\n' "$(_manakan_config_root)"
}

_manakan_targets_dir() {
    printf '%s/targets\n' "$(_manakan_config_root)"
}

_manakan_list_providers() {
    local providers_dir
    providers_dir="$(_manakan_providers_dir)"
    [[ -d "$providers_dir" ]] || return 0

    local file name
    while IFS= read -r -d '' file; do
        name=$(sed -n 's/^name[[:space:]]*=[[:space:]]*"\(.*\)"/\1/p' "$file" | head -n 1)
        [[ -n "$name" ]] && printf '%s\n' "$name"
    done < <(find "$providers_dir" -maxdepth 1 -type f -name '*.toml' -print0 2>/dev/null)
}

_manakan_target_file_use() {
    local file="$1"
    sed -n 's/^use[[:space:]]*=[[:space:]]*"\(.*\)"/\1/p' "$file" | head -n 1
}

_manakan_list_targets() {
    local provider_filter="$1"
    local targets_dir
    targets_dir="$(_manakan_targets_dir)"
    [[ -d "$targets_dir" ]] || return 0

    local file use
    while IFS= read -r -d '' file; do
        use="$(_manakan_target_file_use "$file")"
        if [[ -n "$provider_filter" && "$use" != "$provider_filter" ]]; then
            continue
        fi
        sed -n 's/^\[\([^]]\+\)\][[:space:]]*$/\1/p' "$file"
    done < <(find "$targets_dir" -maxdepth 1 -type f -name '*.toml' -print0 2>/dev/null)
}

_manakan_find_target_file() {
    local target_name="$1"
    local targets_dir
    targets_dir="$(_manakan_targets_dir)"
    [[ -d "$targets_dir" ]] || return 1

    local file
    while IFS= read -r -d '' file; do
        if grep -qE "^\\[$target_name\\]$" "$file"; then
            printf '%s\n' "$file"
            return 0
        fi
    done < <(find "$targets_dir" -maxdepth 1 -type f -name '*.toml' -print0 2>/dev/null)
    return 1
}

_manakan_current_provider() {
    local i
    for ((i = 1; i < COMP_CWORD; i++)); do
        case "${COMP_WORDS[i]}" in
            -p|--provider)
                if (( i + 1 < COMP_CWORD )); then
                    printf '%s\n' "${COMP_WORDS[i+1]}"
                    return 0
                fi
                ;;
            -t|--target)
                if (( i + 1 < COMP_CWORD )); then
                    local target_file
                    target_file="$(_manakan_find_target_file "${COMP_WORDS[i+1]}")" || continue
                    _manakan_target_file_use "$target_file"
                    return 0
                fi
                ;;
        esac
    done
    return 1
}

_manakan_current_target() {
    local i
    for ((i = 1; i < COMP_CWORD; i++)); do
        case "${COMP_WORDS[i]}" in
            -t|--target)
                if (( i + 1 < COMP_CWORD )); then
                    printf '%s\n' "${COMP_WORDS[i+1]}"
                    return 0
                fi
                ;;
        esac
    done
    return 1
}

_manakan_provider_file_by_name() {
    local provider_name="$1"
    local providers_dir
    providers_dir="$(_manakan_providers_dir)"
    [[ -d "$providers_dir" ]] || return 1

    local file name
    while IFS= read -r -d '' file; do
        name=$(sed -n 's/^name[[:space:]]*=[[:space:]]*"\(.*\)"/\1/p' "$file" | head -n 1)
        if [[ "$name" == "$provider_name" ]]; then
            printf '%s\n' "$file"
            return 0
        fi
    done < <(find "$providers_dir" -maxdepth 1 -type f -name '*.toml' -print0 2>/dev/null)
    return 1
}

_manakan_provider_keys() {
    local provider_name="$1"
    [[ -n "$provider_name" ]] || return 0

    local file
    file="$(_manakan_provider_file_by_name "$provider_name")" || return 0

    awk '
        BEGIN { in_headers = 0; in_body = 0 }
        /^\[/ {
            in_headers = ($0 == "[headers]")
            in_body = ($0 == "[body]")
            next
        }
        in_headers || in_body {
            if (match($0, /^[[:space:]]*([A-Za-z0-9_.-]+)[[:space:]]*=/, m)) {
                print m[1]
            }
        }
    ' "$file"
}

_manakan_target_keys() {
    local target_name="$1"
    [[ -n "$target_name" ]] || return 0

    local file
    file="$(_manakan_find_target_file "$target_name")" || return 0

    awk -v target="$target_name" '
        BEGIN { in_target = 0 }
        /^\[/ {
            in_target = ($0 == "[" target "]")
            next
        }
        in_target {
            if (match($0, /^[[:space:]]*([A-Za-z0-9_.-]+)[[:space:]]*=/, m)) {
                print m[1]
            }
        }
    ' "$file"
}

_manakan_input_keys() {
    local provider target
    provider="$(_manakan_current_provider)"
    target="$(_manakan_current_target)"

    {
        _manakan_provider_keys "$provider"
        _manakan_target_keys "$target"
    } | awk '!seen[$0]++'
}

_manakan_complete_input() {
    local cur="$1"
    local prefix key

    if [[ "$cur" == *=* ]]; then
        return 0
    fi

    while IFS= read -r key; do
        [[ -n "$key" ]] || continue
        COMPREPLY+=("${key}=")
    done < <(_manakan_input_keys)
}

_manakan_completions() {
    local cur prev provider
    COMPREPLY=()
    cur="${COMP_WORDS[COMP_CWORD]}"
    prev="${COMP_WORDS[COMP_CWORD-1]}"

    case "$prev" in
        -p|--provider)
            COMPREPLY=($(compgen -W "$(_manakan_list_providers)" -- "$cur"))
            return 0
            ;;
        -t|--target)
            provider="$(_manakan_current_provider)"
            COMPREPLY=($(compgen -W "$(_manakan_list_targets "$provider")" -- "$cur"))
            return 0
            ;;
        -i|--input)
            _manakan_complete_input "$cur"
            return 0
            ;;
    esac

    if [[ "$cur" == -* ]]; then
        COMPREPLY=($(compgen -W "--input -i --provider -p --target -t --help -h --version -v" -- "$cur"))
        return 0
    fi

    return 0
}

complete -F _manakan_completions manakan
