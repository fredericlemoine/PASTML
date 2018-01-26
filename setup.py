from distutils.core import setup, Extension

# the C extension module
pastml_module = Extension('pastml_module', sources=['pastmlpymodule.c',
                                                    'runpastml.c',
                                                    'make_tree.c',
                                                    'lik.c',
                                                    'marginal_lik.c',
                                                    'marginal_approxi.c',
                                                    'output_tree.c',
                                                    'output_states.c',
                                                    'fletcher.c',
                                                    'nrutil.c',
                                                    'golden.c',
                                                    'fletcherJC.c'])

setup(
    name='pastml',
    platform=['Linux', 'Windows', 'Mac OS'],
    classifiers=[
        'Development Status :: 4 - Beta',
        'Environment :: Console',
        'Intended Audience :: Developers',
        'Topic :: Scientific/Engineering :: Bio-Informatics',
        'Topic :: Software Development',
        'Topic :: Software Development :: Libraries :: Python Modules',
    ],
    version='0.2',
    description='Python wrapper for PASTML.',
    maintainer='Anna Zhukova',
    maintainer_email='anna.zhukova@pasteur.fr',
    url='https://github.com/saishikawa/PASTML',
    download_url='https://github.com/saishikawa/PASTML',
    keywords=['PASTML', 'phylogeny', 'ancestral state inference', 'likelihood'],
    ext_modules=[pastml_module]
)