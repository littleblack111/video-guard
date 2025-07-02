#!/bin/bash

shopt -s nullglob
set -x

dialog="${1:-hyprDialog}"
XDG_CONFIG_HOME="${XDG_CONFIG_HOME:-$HOME/.config}"

function guard() {
	inotifywait $1 -e open && (
		$2 $1 &
		guard $1 $2
	)
}

function getClients() {
	LC_ALL=C lsof $1 | awk 'NR>1 {print $1}' | sort -u # can't return smh bash
}

function handler() {
	local clients=$(getClients $1)
	local allowedClients
	if [ -f "$XDG_CONFIG_HOME/videoGuard/allowedClients" ]; then
		allowedClients=$(cat "$XDG_CONFIG_HOME/videoGuard/allowedClients")
	else
		allowedClients=""
	fi
	for i in $clients; do
		for j in ${allowedClients:-""}; do
			if [[ "$i" == "$j" ]]; then
				continue
			else
				killall -19 "$i"
				if [[ $("$dialog" "$1" "$clients") == 0 ]]; then
					killall -18 "$i"
				fi
			fi
		done
	done
}

function hyprDialog() {
	local answer=$(hyprland-dialog --title "Permission request" \
		--text "An application <b>$2</b> is trying to access your camera <b>$1</b>.<br/><br/>Do you want to allow it to do so?" \
		--buttons "Allow;Deny")
	if [ "$answer" == "Allow" ]; then
		echo 0
	else
		echo 1
	fi
}

for i in /dev/video*; do
	guard $i handler &
done
