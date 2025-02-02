# Copyright 2013-2019 Lawrence Livermore National Security, LLC and other
# Spack Project Developers. See the top-level COPYRIGHT file for details.
#
# SPDX-License-Identifier: (Apache-2.0 OR MIT)

from spack import *

import sys
import os
import socket
import glob
import shutil

import llnl.util.tty as tty
from os import environ as env


def cmake_cache_entry(name, value, vtype=None):
    """
    Helper that creates CMake cache entry strings used in
    'host-config' files.
    """
    if vtype is None:
        if value == "ON" or value == "OFF":
            vtype = "BOOL"
        else:
            vtype = "PATH"
    return 'set({0} "{1}" CACHE {2} "")\n\n'.format(name, value, vtype)


class Ascent(Package):
    """Ascent is an open source many-core capable lightweight in situ
    visualization and analysis infrastructure for multi-physics HPC
    simulations."""

    homepage = "https://github.com/Alpine-DAV/ascent"

    maintainers = ['cyrush']

    version('develop',
            git='https://github.com/khuck/ascent.git',
            branch='develop',
            submodules=True,
            preferred=True)

    ###########################################################################
    # package variants
    ###########################################################################

    variant("shared", default=True, description="Build Ascent as shared libs")
    variant('test', default=True, description='Enable Ascent unit tests')

    variant("mpi", default=True, description="Build Ascent MPI Support")

    # variants for language support
    variant("python", default=True, description="Build Ascent Python support")
    variant("fortran", default=True, description="Build Ascent Fortran support")

    # variants for runtime features
    variant("vtkh", default=True,
            description="Build VTK-h filter and rendering support")

    variant("openmp", default=(sys.platform != 'darwin'),
            description="build openmp support")
    variant("cuda", default=False, description="Build cuda support")
    variant("mfem", default=False, description="Build MFEM filter support")
    variant("adios", default=False, description="Build Adios filter support")

    # variants for dev-tools (docs, etc)
    variant("doc", default=False, description="Build Conduit's documentation")

    ###########################################################################
    # package dependencies
    ###########################################################################

    depends_on("cmake@3.9.2:3.9.999", type='build')
    depends_on("conduit~python", when="~python")
    depends_on("conduit+python", when="+python+shared")
    depends_on("conduit~shared~python", when="~shared")
    depends_on("perfstubs")

    #######################
    # Python
    #######################
    # we need a shared version of python b/c linking with static python lib
    # causes duplicate state issues when running compiled python modules.
    depends_on("python+shared", when="+python+shared")
    extends("python", when="+python+shared")
    depends_on("py-numpy", when="+python+shared", type=('build', 'run'))

    #######################
    # MPI
    #######################
    depends_on("mpi", when="+mpi")
    # use old version of mpi4py to avoid build issues with cython
    depends_on("py-mpi4py@2.0.0:2.9.999", when="+mpi+python+shared")

    #############################
    # TPLs for Runtime Features
    #############################

    # uberenv overrides to use ascent_ver
    depends_on("vtkh@ascent_ver",             when="+vtkh")
    depends_on("vtkh@ascent_ver~openmp",      when="+vtkh~openmp")
    depends_on("vtkh@ascent_ver+cuda+openmp", when="+vtkh+cuda+openmp")
    depends_on("vtkh@ascent_ver+cuda~openmp", when="+vtkh+cuda~openmp")

    depends_on("vtkh@ascent_ver~shared",             when="~shared+vtkh")
    depends_on("vtkh@ascent_ver~shared~openmp",      when="~shared+vtkh~openmp")
    depends_on("vtkh@ascent_ver~shared+cuda",        when="~shared+vtkh+cuda")
    depends_on("vtkh@ascent_ver~shared+cuda~openmp", when="~shared+vtkh+cuda~openmp")

    # mfem
    depends_on("mfem+shared+mpi+conduit", when="+shared+mfem+mpi")
    depends_on("mfem~shared+mpi+conduit", when="~shared+mfem+mpi")

    depends_on("mfem+shared~mpi+conduit", when="+shared+mfem~mpi")
    depends_on("mfem~shared~mpi+conduit", when="~shared+mfem~mpi")

    depends_on("adios", when="+adios")

    #######################
    # Documentation related
    #######################
    depends_on("py-sphinx", when="+python+doc", type='build')

    def setup_environment(self, spack_env, run_env):
        spack_env.set('CTEST_OUTPUT_ON_FAILURE', '1')

    def install(self, spec, prefix):
        """
        Build and install Ascent.
        """
        with working_dir('spack-build', create=True):
            py_site_pkgs_dir = None
            if "+python" in spec:
                py_site_pkgs_dir = site_packages_dir

            host_cfg_fname = self.create_host_config(spec,
                                                     prefix,
                                                     py_site_pkgs_dir)
            cmake_args = []
            # if we have a static build, we need to avoid any of
            # spack's default cmake settings related to rpaths
            # (see: https://github.com/LLNL/spack/issues/2658)
            if "+shared" in spec:
                cmake_args.extend(std_cmake_args)
            else:
                for arg in std_cmake_args:
                    if arg.count("RPATH") == 0:
                        cmake_args.append(arg)
            cmake_args.extend(["-C", host_cfg_fname, "../src"])
            print("Configuring Ascent...")
            cmake(*cmake_args)
            print("Building Ascent...")
            make()
            # run unit tests if requested
            if "+test" in spec and self.run_tests:
                print("Running Ascent Unit Tests...")
                make("test")
            print("Installing Ascent...")
            make("install")
            # install copy of host config for provenance
            install(host_cfg_fname, prefix)

    @run_after('install')
    @on_package_attributes(run_tests=True)
    def check_install(self):
        """
        Checks the spack install of ascent using ascents's
        using-with-cmake example
        """
        print("Checking Ascent installation...")
        spec = self.spec
        install_prefix = spec.prefix
        example_src_dir = join_path(install_prefix,
                                    "examples",
                                    "ascent",
                                    "using-with-cmake")
        print("Checking using-with-cmake example...")
        with working_dir("check-ascent-using-with-cmake-example",
                         create=True):
            cmake_args = ["-DASCENT_DIR={0}".format(install_prefix),
                          "-DCONDUIT_DIR={0}".format(spec['conduit'].prefix),
                          "-DVTKM_DIR={0}".format(spec['vtkm'].prefix),
                          "-DVTKH_DIR={0}".format(spec['vtkh'].prefix),
                          example_src_dir]
            cmake(*cmake_args)
            make()
            example = Executable('./ascent_render_example')
            example()
        print("Checking using-with-make example...")
        example_src_dir = join_path(install_prefix,
                                    "examples",
                                    "ascent",
                                    "using-with-make")
        example_files = glob.glob(join_path(example_src_dir, "*"))
        with working_dir("check-ascent-using-with-make-example",
                         create=True):
            for example_file in example_files:
                shutil.copy(example_file, ".")
            make("ASCENT_DIR={0}".format(install_prefix))
            example = Executable('./ascent_render_example')
            example()

    def create_host_config(self, spec, prefix, py_site_pkgs_dir=None):
        """
        This method creates a 'host-config' file that specifies
        all of the options used to configure and build ascent.

        For more details about 'host-config' files see:
            http://ascent.readthedocs.io/en/latest/BuildingAscent.html

        Note:
          The `py_site_pkgs_dir` arg exists to allow a package that
          subclasses this package provide a specific site packages
          dir when calling this function. `py_site_pkgs_dir` should
          be an absolute path or `None`.

          This is necessary because the spack `site_packages_dir`
          var will not exist in the base class. For more details
          on this issue see: https://github.com/spack/spack/issues/6261
        """

        #######################
        # Compiler Info
        #######################
        c_compiler = env["SPACK_CC"]
        cpp_compiler = env["SPACK_CXX"]
        f_compiler = None

        if self.compiler.fc:
            # even if this is set, it may not exist so do one more sanity check
            f_compiler = env["SPACK_FC"]

        #######################################################################
        # By directly fetching the names of the actual compilers we appear
        # to doing something evil here, but this is necessary to create a
        # 'host config' file that works outside of the spack install env.
        #######################################################################

        sys_type = spec.architecture
        # if on llnl systems, we can use the SYS_TYPE
        if "SYS_TYPE" in env:
            sys_type = env["SYS_TYPE"]

        ##############################################
        # Find and record what CMake is used
        ##############################################

        if "+cmake" in spec:
            cmake_exe = spec['cmake'].command.path
        else:
            cmake_exe = which("cmake")
            if cmake_exe is None:
                msg = 'failed to find CMake (and cmake variant is off)'
                raise RuntimeError(msg)
            cmake_exe = cmake_exe.path

        host_cfg_fname = "%s-%s-%s-ascent.cmake" % (socket.gethostname(),
                                                    sys_type,
                                                    spec.compiler)

        cfg = open(host_cfg_fname, "w")
        cfg.write("##################################\n")
        cfg.write("# spack generated host-config\n")
        cfg.write("##################################\n")
        cfg.write("# {0}-{1}\n".format(sys_type, spec.compiler))
        cfg.write("##################################\n\n")

        # Include path to cmake for reference
        cfg.write("# cmake from spack \n")
        cfg.write("# cmake executable path: %s\n\n" % cmake_exe)

        #######################
        # Compiler Settings
        #######################
        cfg.write("#######\n")
        cfg.write("# using %s compiler spec\n" % spec.compiler)
        cfg.write("#######\n\n")
        cfg.write("# c compiler used by spack\n")
        cfg.write(cmake_cache_entry("CMAKE_C_COMPILER", c_compiler))
        cfg.write("# cpp compiler used by spack\n")
        cfg.write(cmake_cache_entry("CMAKE_CXX_COMPILER", cpp_compiler))

        cfg.write("# fortran compiler used by spack\n")
        if "+fortran" in spec and f_compiler is not None:
            cfg.write(cmake_cache_entry("ENABLE_FORTRAN", "ON"))
            cfg.write(cmake_cache_entry("CMAKE_Fortran_COMPILER",
                                        f_compiler))
        else:
            cfg.write("# no fortran compiler found\n\n")
            cfg.write(cmake_cache_entry("ENABLE_FORTRAN", "OFF"))

        # shared vs static libs
        if "+shared" in spec:
            cfg.write(cmake_cache_entry("BUILD_SHARED_LIBS", "ON"))
        else:
            cfg.write(cmake_cache_entry("BUILD_SHARED_LIBS", "OFF"))

        #######################
        # Unit Tests
        #######################
        if "+test" in spec:
            cfg.write(cmake_cache_entry("ENABLE_TESTS", "ON"))
        else:
            cfg.write(cmake_cache_entry("ENABLE_TESTS", "OFF"))

        #######################################################################
        # Core Dependencies
        #######################################################################

        #######################
        # Conduit
        #######################

        cfg.write("# conduit from spack \n")
        cfg.write(cmake_cache_entry("CONDUIT_DIR", spec['conduit'].prefix))

        cfg.write("# perfstubs from spack \n")
        cfg.write(cmake_cache_entry("PERFSTUBS_DIR", spec['perfstubs'].prefix))

        #######################################################################
        # Optional Dependencies
        #######################################################################

        #######################
        # Python
        #######################

        cfg.write("# Python Support\n")

        if "+python" in spec and "+shared" in spec:
            cfg.write("# Enable python module builds\n")
            cfg.write(cmake_cache_entry("ENABLE_PYTHON", "ON"))
            cfg.write("# python from spack \n")
            cfg.write(cmake_cache_entry("PYTHON_EXECUTABLE",
                      spec['python'].command.path))
            # only set dest python site packages dir if passed
            if py_site_pkgs_dir:
                cfg.write(cmake_cache_entry("PYTHON_MODULE_INSTALL_PREFIX",
                                            py_site_pkgs_dir))
        else:
            cfg.write(cmake_cache_entry("ENABLE_PYTHON", "OFF"))

        if "+doc" in spec and "+python" in spec:
            cfg.write(cmake_cache_entry("ENABLE_DOCS", "ON"))

            cfg.write("# sphinx from spack \n")
            sphinx_build_exe = join_path(spec['py-sphinx'].prefix.bin,
                                         "sphinx-build")
            cfg.write(cmake_cache_entry("SPHINX_EXECUTABLE", sphinx_build_exe))
        else:
            cfg.write(cmake_cache_entry("ENABLE_DOCS", "OFF"))

        #######################
        # MPI
        #######################

        cfg.write("# MPI Support\n")

        if "+mpi" in spec:
            cfg.write(cmake_cache_entry("ENABLE_MPI", "ON"))
            cfg.write(cmake_cache_entry("MPI_C_COMPILER", spec['mpi'].mpicc))
            cfg.write(cmake_cache_entry("MPI_CXX_COMPILER",
                                        spec['mpi'].mpicxx))
            cfg.write(cmake_cache_entry("MPI_Fortran_COMPILER",
                                        spec['mpi'].mpifc))
            mpiexe_bin = join_path(spec['mpi'].prefix.bin, 'mpiexec')
            if os.path.isfile(mpiexe_bin):
                # starting with cmake 3.10, FindMPI expects MPIEXEC_EXECUTABLE
                # vs the older versions which expect MPIEXEC
                if self.spec["cmake"].satisfies('@3.10:'):
                    cfg.write(cmake_cache_entry("MPIEXEC_EXECUTABLE",
                                                mpiexe_bin))
                else:
                    cfg.write(cmake_cache_entry("MPIEXEC",
                                                mpiexe_bin))
        else:
            cfg.write(cmake_cache_entry("ENABLE_MPI", "OFF"))

        #######################
        # CUDA
        #######################

        cfg.write("# CUDA Support\n")

        if "+cuda" in spec:
            cfg.write(cmake_cache_entry("ENABLE_CUDA", "ON"))
        else:
            cfg.write(cmake_cache_entry("ENABLE_CUDA", "OFF"))

        if "+openmp" in spec:
            cfg.write(cmake_cache_entry("ENABLE_OPENMP", "ON"))
        else:
            cfg.write(cmake_cache_entry("ENABLE_OPENMP", "OFF"))

        #######################
        # VTK-h (and deps)
        #######################

        cfg.write("# vtk-h support \n")

        if "+vtkh" in spec:
            cfg.write("# vtk-m from spack\n")
            cfg.write(cmake_cache_entry("VTKM_DIR", spec['vtkm'].prefix))

            cfg.write("# vtk-h from spack\n")
            cfg.write(cmake_cache_entry("VTKH_DIR", spec['vtkh'].prefix))

        else:
            cfg.write("# vtk-h not built by spack \n")

        #######################
        # MFEM
        #######################
        if "+mfem" in spec:
            cfg.write("# mfem from spack \n")
            cfg.write(cmake_cache_entry("MFEM_DIR", spec['mfem'].prefix))
        else:
            cfg.write("# mfem not built by spack \n")

        #######################
        # Adios
        #######################

        cfg.write("# adios support\n")

        if "+adios" in spec:
            cfg.write(cmake_cache_entry("ADIOS_DIR", spec['adios'].prefix))
        else:
            cfg.write("# adios not built by spack \n")

        cfg.write("##################################\n")
        cfg.write("# end spack generated host-config\n")
        cfg.write("##################################\n")
        cfg.close()

        host_cfg_fname = os.path.abspath(host_cfg_fname)
        tty.info("spack generated conduit host-config file: " + host_cfg_fname)
        return host_cfg_fname
