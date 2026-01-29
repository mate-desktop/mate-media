#!/usr/bin/bash

set -eo pipefail

# Use grouped output messages
infobegin() {
	echo "::group::${1}"
}
infoend() {
	echo "::endgroup::"
}

# Required packages on Ubuntu
requires=(
	ccache # Use ccache to speed up build
	meson  # Used for meson build
)

# https://git.launchpad.net/ubuntu/+source/mate-media/tree/debian/control
requires+=(
	autoconf-archive
	autopoint
	gcc
	git
	libcanberra-gtk3-dev
	libglib2.0-dev
	libgtk-3-dev
	libgtk-layer-shell-dev
	libmate-desktop-dev
	libmate-panel-applet-dev
	libmatemixer-dev
	libxml2-dev
	make
	mate-common
	iso-codes
	gobject-introspection
	libgirepository1.0-dev
)

infobegin "Update system"
apt-get update -y
infoend

infobegin "Install dependency packages"
env DEBIAN_FRONTEND=noninteractive \
	apt-get install --assume-yes \
	${requires[@]}
infoend
