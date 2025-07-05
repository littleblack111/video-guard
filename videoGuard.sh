#!/bin/bash

shopt -s nullglob

if ! command -v video-guard-helper &>/dev/null; then
	echo "video-guard-helper is not installed. Please install it first."
	exit 1
fi

dialog="${1:-hyprDialog}"
XDG_CONFIG_HOME="${XDG_CONFIG_HOME:-$HOME/.config}"

function handler() {
	local client=$(ps -p $2 -o comm= 2>/dev/null) # pid may be null but we don't care
	local allowedClients
	local ignoredCamera
	if [ -f "$XDG_CONFIG_HOME/videoGuard/allowedClients" ]; then
		allowedClients=$(cat "$XDG_CONFIG_HOME/videoGuard/allowedClients")
	else
		allowedClients=""
	fi
	if [ -f "$XDG_CONFIG_HOME/videoGuard/ignoredCamera" ]; then
		ignoredCamera=$(cat "$XDG_CONFIG_HOME/videoGuard/ignoredCamera")
	else
		ignoredCamera=""
	fi
	if [[ $1 == "$ignoredCamera" ]]; then
		kill -18 "$2" 2>/dev/null # pid may be null but we don't care
		return
	fi
	if [[ $client == "" ]]; then # can still happen if another program with kernel access is using the camera
		if [[ "$2" == "" ]]; then
			$dialog 0 "$1" "<b>Unknown</b> application"
		else
			if [[ $($dialog 1 "$1" "with PID <b>$2</b>") ]]; then
				kill -18 "$2"
			fi
		fi
		return
	fi
	for j in $allowedClients; do
		if [[ "$client" == "$j" ]]; then
			$dialog 0 "$1" "allowed application <b>$client</b>"
			kill -18 "$2"
			return
		fi
	done
	kill -19 "$2"
	if [[ $($dialog 1 "$1" "<b>$client</b>") == 0 ]]; then
		kill -18 "$2"
	fi
}

function guard() {
	while true; do
		handler "$1" "$(video-guard-helper open "${1: -1}")"
	done
}

function hyprDialog() {
	if [ $1 -eq 0 ]; then
		notify-send -e "Video Accessed" "An $3 is accessing your camera <b>$2</b>."
		return 0
	fi
	local answer=$(hyprland-dialog --title "Permission request" \
		--text "An application $3 is trying to access your camera <b>$2</b>.<br/><br/>Do you want to allow it to do so?" \
		--buttons "Allow;Deny")
	if [ "$answer" == "Allow" ]; then
		echo 0
	else
		echo 1
	fi
}

for i in /dev/video*; do
	guard $i &
done
