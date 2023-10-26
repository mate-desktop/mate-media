xml_data = configuration_data()
xml_data.set('datadir', mm_datadir)
xml_in_file = configure_file(
  input : 'mate-sounds-default.xml.in.in',
  output : 'mate-sounds-default.xml.in',
  configuration : xml_data
)

i18n.merge_file(
  input: xml_in_file,
  output: 'mate-sounds-default.xml',
  po_dir: po_dir,
  data_dirs: its_dir,
  install: true,
  install_dir: join_paths(mm_pkgdatadir, 'sounds')
)

oggs = [
  'bark.ogg',
  'drip.ogg',
  'glass.ogg',
  'sonar.ogg'
]
foreach ogg: oggs
    install_data(
    ogg,
    install_dir: join_paths(mm_datadir, 'sounds', 'mate', 'default', 'alerts')
  )
endforeach
