#!/bin/bash

shopt -s nullglob

dialog="${1:-hyprDialog}"
XDG_CONFIG_HOME="${XDG_CONFIG_HOME:-$HOME/.config}"

function guard() {
	inotifywait $1 -e open && (
		$2 $1 &
		guard $1 $2
	)
}

function getClient() {
	video-guard-helper $1
}

function handler() {
	local client=$(ps -p $(getClient "$1") -o comm=)
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
		return 0
	fi
	if [[ $client == "" ]]; then # shouldn't happen now as our helper is ran in kernel
		$dialog 0 "$1" "Unknown"
	fi
	for j in ${allowedClients:-""}; do
		if [[ "$client" == "$j" ]]; then
			$dialog 2 "$1" "$client"
		else
			killall -19 "$client"
			if [[ $("$dialog" 1 "$1" "$client") == 0 ]]; then
				killall -18 "$client"
			fi
		fi
	done
}

function hyprDialog() {
	if [ $1 -eq 0 ]; then
		notify-send -e "Video Accessed" "An <b>$3</b> application is accessing your camera <b>$2</b>."
		return 0
	elif [ $1 -eq 2 ]; then
		notify-send -e "Video Accessed" "An allowed <b>$3</b> application is accessing your camera <b>$2</b>."
		return 0
	fi
	local answer=$(hyprland-dialog --title "Permission request" \
		--text "An application <b>$3</b> is trying to access your camera <b>$2</b>.<br/><br/>Do you want to allow it to do so?" \
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
