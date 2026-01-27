#!/usr/bin/bash

# Use grouped output messages
infobegin() {
	echo "::group::${1}"
}
infoend() {
	echo "::endgroup::"
}

# Required packages on Fedora
requires=(
	ccache # Use ccache to speed up build
	meson  # Used for meson build
)

requires+=(
	autoconf-archive
	gcc
	desktop-file-utils
	git
	gtk3-devel
	gtk-layer-shell-devel
	libmatemixer-devel
	libxml2-devel
	libcanberra-devel
	make
	mate-common
	mate-desktop-devel
	mate-panel-devel
	redhat-rpm-config
	iso-codes-devel
	gobject-introspection-devel
)

infobegin "Update system"
dnf update -y
infoend

infobegin "Install dependency packages"
dnf install -y ${requires[@]}
infoend
