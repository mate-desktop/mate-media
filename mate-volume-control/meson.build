po_dir = join_paths(meson.source_root(), 'po')

cflags = [
  '-DMATELOCALEDIR="@0@"'.format(mm_localedir),
  '-DLOCALE_DIR="@0@"'.format(mm_localedir),
  '-DICON_DATA_DIR="@0@"'.format(mm_iconsdir),
  '-DSOUND_SET_DIR="@0@"'.format(mm_soundsdir),
  '-DI_KNOW_THE_DEVICEKIT_POWER_API_IS_SUBJECT_TO_CHANGE',
]
deps = [
  glib,
  gtk,
  matemixer
]
if enable_wayland == 'yes'
  deps += [gdkwayland, waylandclient, gls]
endif

libmvc_static = static_library(
  'libmatevolumecontrol',
  sources: 'gvc-channel-bar.c',
  include_directories: config_inc,
  dependencies: deps,
  c_args: cflags,
  install: false,
)

if enable_applet
  desktop_data = configuration_data()
  
  if enable_process
    desktop_data.set('APPLET_LOCATION', join_paths(mm_libdir, 'libmate-volume-control-applet.so'))
    desktop_data.set('IN_PROCESS', 'true')
    
    libmvca_shared = library(
      'libmate-volume-control-applet',
      sources: [
        'gvc-stream-applet-icon.c',
        'gvc-applet.c',
        'applet-main.c'
      ],
      include_directories: config_inc,
      dependencies: [
        deps,
        matepanel,
        md,
        canberra,
        libm
      ],
      link_with :libmvc_static,
      c_args: cflags,
      install: true,
      install_dir : mm_libdir, 
    )

  else
    desktop_data.set('APPLET_LOCATION', join_paths(mm_execdir, 'mate-volume-control-applet'))
    desktop_data.set('IN_PROCESS', 'flase')
    
    service_data = configuration_data()
    service_data.set('LOCATION', join_paths(mm_execdir, 'mate-volume-control-applet'))
    configure_file(
      input : 'org.mate.panel.applet.GvcAppletFactory.service.in',
      output : 'org.mate.panel.applet.GvcAppletFactory.service',
      configuration : service_data,
      install : true,
      install_dir : join_paths(mm_datadir, 'dbus-1/services')
    )
    executable(
      'mate-volume-control-applet',
      sources : [
        'gvc-stream-applet-icon.c',
        'gvc-applet.c',
        'applet-main.c'
      ],
      include_directories: config_inc,
      dependencies : [
        deps,
        matepanel,
        md,
        canberra,
        libm
      ],
      link_with :libmvc_static,
      c_args : cflags,
      install : true,
      install_dir : mm_execdir
    )
  endif

  desktop_in_file = configure_file(
    input : 'org.mate.applets.GvcApplet.mate-panel-applet.desktop.in.in',
    output : 'org.mate.applets.GvcApplet.mate-panel-applet.desktop.in',
    configuration : desktop_data
  )
  i18n.merge_file(
    input : desktop_in_file,
    output : 'org.mate.applets.GvcApplet.mate-panel-applet.desktop',
    type : 'desktop',
    po_dir : po_dir,
    install : true,
    install_dir : join_paths(mm_datadir, 'mate-panel', 'applets')
  )

endif

if enable_statusicon
  executable(
    'mate-volume-control-status-icon',
    sources : [
      'gvc-stream-status-icon.c',
      'gvc-status-icon.c',
      'status-icon-main.c'
    ],
    include_directories: config_inc,
    dependencies : [
      deps,
      md,
      canberra,
      libm
    ],
    link_with :libmvc_static,
    c_args : cflags,
    install : true,
    install_dir : get_option('bindir')
)
endif
executable(
  'mate-volume-control',
  sources : [
    'gvc-balance-bar.c',
    'gvc-level-bar.c',
    'gvc-combo-box.c',
    'gvc-sound-theme-chooser.c',
    'gvc-speaker-test.c',
    'gvc-utils.c',
    'sound-theme-file-utils.c',
    'gvc-mixer-dialog.c',
    'dialog-main.c'
  ],
  include_directories : config_inc,
  dependencies : [
    libm,
    canberra,
    md,
    libxml,
    deps
  ],
  link_with :libmvc_static,
  c_args : cflags,
  install : true,
  install_dir : get_option('bindir')
)

