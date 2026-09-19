project(
  'dispatch',
  'cpp',
  version: '0.2.0',
  license: 'Unlicense',
  meson_version: '>=0.56',
  default_options: [
    'cpp_std=c++17',
    'warning_level=2',
    'prefix=/usr',
  ],
)

gtkmm = dependency('gtkmm-3.0')
libxml = dependency('libxml-2.0')
libsoup = dependency('libsoup-3.0')
fontconfig = dependency('fontconfig')
etpan = dependency('libetpan')
gmime = dependency('gmime-3.0')

conf = configuration_data()
conf.set_quoted('VERSION', meson.project_version())
conf.set_quoted('APP_ID', 'org.gmgauthier.Dispatch')
conf.set_quoted('DATADIR', get_option('prefix') / get_option('datadir') / 'dispatch')
conf.set_quoted('SOURCE_ROOT', meson.project_source_root())

configure_file(
  output: 'config.hpp',
  configuration: conf,
)

inc = include_directories('.', 'src')

executable(
  'dispatch',
  'src/main.cpp',
  'src/application.cpp',
  'src/paths.cpp',
  'src/main_window.cpp',
  'src/article_window.cpp',
  'src/about_dialog.cpp',
  'src/subscribe_dialog.cpp',
  'src/account_dialog.cpp',
  'src/feed.cpp',
  'src/fetch.cpp',
  'src/settings.cpp',
  'src/opml.cpp',
  'src/body_view.cpp',
  'src/font_dialog.cpp',
  'src/mail_store.cpp',
  'src/mail_parse.cpp',
  'src/mail_imap.cpp',
  'src/mail_smtp.cpp',
  'src/compose_window.cpp',
  'src/compose_format.cpp',
  'src/ephemeris_contacts.cpp',
  include_directories: inc,
  dependencies: [gtkmm, libxml, libsoup, fontconfig, etpan, gmime],
  install: true,
  install_dir: get_option('bindir'),
)

install_data(
  'data/dispatch.desktop',
  install_dir: get_option('datadir') / 'applications',
)

install_data(
  'data/skin/lcos/lcos.css',
  install_dir: get_option('datadir') / 'dispatch' / 'skin' / 'lcos',
)

install_data(
  'brand/icon-tile.svg',
  install_dir: get_option('datadir') / 'dispatch' / 'brand',
)

install_data(
  'brand/icon-tile.svg',
  rename: 'dispatch.svg',
  install_dir: get_option('datadir') / 'icons' / 'hicolor' / 'scalable' / 'apps',
)
