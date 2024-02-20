project('mate-media', 'c',
  version : '1.28.0',
  default_options : ['warning_level=1'],
  meson_version : '>=0.50.0'
)

conf = configuration_data()

prefix = get_option('prefix')
mm_execdir = join_paths(prefix, get_option('libexecdir'))
mm_libdir = join_paths(prefix, get_option('libdir'))
mm_datadir = join_paths(prefix, get_option('datadir'))
mm_localedir = join_paths(prefix, get_option('localedir'))
mm_bindir = join_paths(prefix, get_option('bindir'))
mm_sbindir = join_paths(prefix, get_option('sbindir'))
mm_mandir = join_paths(prefix, get_option('mandir'))
mm_desktopdir =  join_paths(mm_datadir, 'applications')
mm_pkgdatadir =  join_paths(mm_datadir, meson.project_name())
mm_iconsdir =  join_paths(mm_pkgdatadir, 'icons')
mm_soundsdir =  join_paths(mm_pkgdatadir, 'sounds')

# get suported warning flags
test_args = [
  '-Waggregate-return',
  '-Warray-bounds',
  '-Wcast-align',
  '-Wclobbered',
  '-Wdeclaration-after-statement',
  '-Wempty-body',
  '-Wextra',
  '-Wformat=2',
  '-Wformat-nonliteral',
  '-Wformat-security',
  '-Wformat-signedness',
  '-Wignored-qualifiers',
  '-Wimplicit-function-declaration',
  '-Winit-self',
  '-Winline',
  '-Wmissing-declarations',
  '-Wmissing-format-attribute',
  '-Wmissing-include-dirs',
  '-Wmissing-noreturn',
  '-Wmissing-parameter-type',
  '-Wmissing-prototypes',
  '-Wnested-externs',
  '-Wno-discarded-qualifiers',
  '-Wno-missing-field-initializers',
  '-Wno-strict-aliasing',
  '-Wno-suggest-attribute=format',
  '-Wno-unused-parameter',
  '-Wold-style-definition',
  '-Woverride-init',
  '-Wpacked',
  '-Wpointer-arith',
  '-Wredundant-decls',
  '-Wreturn-type',
  '-Wshadow',
  '-Wsign-compare',
  '-Wstrict-aliasing',
  '-Wstrict-prototypes',
  '-Wswitch-default',
  '-Wtype-limits',
  '-Wundef',
  '-Wuninitialized',
  '-Wunused-but-set-variable',
  '-Wwrite-strings'
]
cc = meson.get_compiler('c')
foreach arg: test_args
  if cc.has_argument(arg)
    add_project_arguments(arg, language : 'c')
  endif
endforeach

common_flags = [
  '-DHAVE_CONFIG_H',
]

add_project_arguments(common_flags, language: 'c')
# enable full RELRO where possible
# FIXME: until https://github.com/mesonbuild/meson/issues/1140 is fixed
test_link_args = [
  '-Wl,-z,relro',
  '-Wl,-z,now',
]
foreach arg: test_link_args
  if cc.has_link_argument(arg)
    add_project_link_arguments(arg, language : 'c')
  endif
endforeach

enable_statusicon = get_option('statusicon')
enable_applet = get_option('panelapplet')
enable_wayland = get_option('wayland')
enable_process = get_option('in-process')

if enable_wayland == 'auto'
  if enable_process
    enable_wayland = 'yes'
  else
    enable_wayland = 'no'
  endif
elif enable_wayland == 'yes'
  if not enable_process
    error ('enable-wayland is yes requires enable-in-process is true')
  endif
endif

gtk = dependency('gtk+-3.0', version : '>= 3.22.0')
glib = dependency('glib-2.0', version : '>= 2.50.0')
gdk = dependency('gdk-3.0', version : '>= 3.22.0')
matemixer = dependency('libmatemixer', version : '>= 1.10.0')
canberra = dependency('libcanberra-gtk3', version : '>= 0.13')
md = dependency('mate-desktop-2.0', version : '>= 1.27.1')
matepanel = dependency('libmatepanelapplet-4.0', version : '>= 1.17.0',required: enable_applet)
libxml = dependency('libxml-2.0')
libm = cc.find_library('m', required: false)

if enable_wayland == 'yes'
  gls = dependency('gtk-layer-shell-0', version : '>= 0.6')
  waylandclient = dependency('wayland-client')
  gdkwayland = dependency('gdk-wayland-3.0', version : '>= 3.22.0')
  conf.set('ENABLE_WAYLAND', 1)
endif
if enable_process
  conf.set('IN_PROCESS', 1)
endif
if enable_statusicon
  conf.set('ENABLE_STATUSICON', 1)
endif
if enable_applet
  conf.set('ENABLE_PANELAPPLET', 1)
endif

gnome = import('gnome')
i18n = import('i18n')

conf.set_quoted('GETTEXT_PACKAGE', meson.project_name())
conf.set_quoted('LOCALEDIR', mm_localedir)
conf.set_quoted('DATADIR', mm_datadir)

conf.set_quoted('PACKAGE_URL', 'https://mate-desktop.org')
conf.set_quoted('VERSION', meson.project_version())
conf.set_quoted('PACKAGE_VERSION', meson.project_version())

configure_file(
  output : 'config.h',
  configuration : conf
)
config_inc = include_directories('.')

subdir('man')
subdir('mate-volume-control')
subdir('po')
subdir('data')

summary = [
  'configuration summary:',
  '',
  '                         project: @0@ @1@'.format(meson.project_name(), meson.project_version()),
  '                          prefix: @0@'.format(prefix),
  '            Building status icon: @0@'.format(enable_statusicon),
  '           Building panel applet: @0@'.format(enable_applet),
  '             Building in-process: @0@'.format(enable_process),
  '                 Wayland support: @0@'.format(enable_wayland),
  ''
]
message('\n'.join(summary))

meson.add_install_script('build-aux/meson_post_install.sh')
