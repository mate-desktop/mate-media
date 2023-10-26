po_dir = join_paths(meson.source_root(), 'po')
its_dir = join_paths(meson.source_root())
subdir('icons')
subdir('sounds')
# .desktop file

desktop_data = configuration_data()
desktop_data.set('VERSION', meson.project_version())
desktop_in_file = configure_file(
  input : 'mate-volume-control.desktop.in.in',
  output : 'mate-volume-control.desktop.in',
  configuration : desktop_data
)

i18n.merge_file(
  input : desktop_in_file,
  output : 'mate-volume-control.desktop',
  type : 'desktop',
  po_dir : po_dir,
  install : true,
  install_dir : join_paths(mm_datadir, 'applications')
)

desktop = 'mate-volume-control-status-icon.desktop'

i18n.merge_file(
  type: 'desktop',
  input: desktop + '.in',
  output: desktop,
  po_dir: po_dir,
  install: true,
  install_dir: join_paths(get_option('sysconfdir'), 'xdg', 'autostart')
)
