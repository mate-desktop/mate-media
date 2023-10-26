status_icons = [
  ['16x16', 'audio-input-microphone-high.png', 'audio-input-microphone-low.png', 'audio-input-microphone-medium.png', 'audio-input-microphone-muted.png'],
  ['22x22', 'audio-input-microphone-high.png', 'audio-input-microphone-low.png', 'audio-input-microphone-medium.png', 'audio-input-microphone-muted.png'],
  ['24x24', 'audio-input-microphone-high.png', 'audio-input-microphone-low.png', 'audio-input-microphone-medium.png', 'audio-input-microphone-muted.png'],
  ['32x32', 'audio-input-microphone-high.png', 'audio-input-microphone-low.png', 'audio-input-microphone-medium.png', 'audio-input-microphone-muted.png'],
]

foreach icon: status_icons
  install_data(
    join_paths(icon[0], 'status', icon[1]),
    install_dir: join_paths(mm_iconsdir, 'hicolor', icon[0], 'status')
  )
  install_data(
    join_paths(icon[0], 'status', icon[2]),
    install_dir: join_paths(mm_iconsdir, 'hicolor', icon[0], 'status')
  )
  install_data(
    join_paths(icon[0], 'status', icon[3]),
    install_dir: join_paths(mm_iconsdir, 'hicolor', icon[0], 'status')
  )
  install_data(
    join_paths(icon[0], 'status', icon[4]),
    install_dir: join_paths(mm_iconsdir, 'hicolor', icon[0], 'status')
  )
endforeach

scalable_icons = [
  'audio-input-microphone-high.svg',
  'audio-input-microphone-low.svg',
  'audio-input-microphone-medium.svg',
  'audio-input-microphone-muted.svg'
]

foreach icon: scalable_icons
    install_data(
    join_paths('scalable', 'status', icon),
    install_dir: join_paths(mm_iconsdir, 'hicolor', 'scalable', 'status')
  )
endforeach

devices_icons = [
  'audio-speaker-center-back.svg',
  'audio-speaker-center-back-testing.svg',
  'audio-speaker-center.svg',
  'audio-speaker-center-testing.svg',
  'audio-speaker-left-back.svg',
  'audio-speaker-left-back-testing.svg',
  'audio-speaker-left-side.svg',
  'audio-speaker-left-side-testing.svg',
  'audio-speaker-left.svg',
  'audio-speaker-left-testing.svg',
  'audio-speaker-right-back.svg',
  'audio-speaker-right-back-testing.svg',
  'audio-speaker-right-side.svg',
  'audio-speaker-right-side-testing.svg',
  'audio-speaker-right.svg',
  'audio-speaker-right-testing.svg',
  'audio-subwoofer.svg',
  'audio-subwoofer-testing.svg',
]

foreach icon: devices_icons
    install_data(
    join_paths('scalable', 'devices', icon),
    install_dir: join_paths(mm_iconsdir, 'hicolor', '48x48', 'devices')
  )
endforeach
