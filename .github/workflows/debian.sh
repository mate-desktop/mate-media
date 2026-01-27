#!/usr/bin/bash

# Use grouped output messages
infobegin() {
	echo "::group::${1}"
}
infoend() {
	echo "::endgroup::"
}

# Required packages on Debian
requires=(
	ccache # Use ccache to speed up build
	meson  # Used for meson build
)

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
apt-get update -qq
infoend

infobegin "Install dependency packages"
env DEBIAN_FRONTEND=noninteractive \
	apt-get install --assume-yes \
	${requires[@]}
infoend
