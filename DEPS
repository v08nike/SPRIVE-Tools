use_relative_paths = True

vars = {
  'github': 'https://github.com',

  'effcee_revision': 'ddf5e2bb92957dc8a12c5392f8495333d6844133',
  'googletest_revision': '548b13dc3c02b93f60eeff9a0cc6e11c1ea722ca',
  're2_revision': '0c5616df9c0aaa44c9440d87422012423d91c7d1',
  'spirv_headers_revision': 'b765c355f488837ca4c77980ba69484f3ff277f5',
}

deps = {
  'external/effcee':
      Var('github') + '/google/effcee.git@' + Var('effcee_revision'),

  'external/googletest':
      Var('github') + '/google/googletest.git@' + Var('googletest_revision'),

  'external/re2':
      Var('github') + '/google/re2.git@' + Var('re2_revision'),

  'external/spirv-headers':
      Var('github') +  '/KhronosGroup/SPIRV-Headers.git@' +
          Var('spirv_headers_revision'),
}

