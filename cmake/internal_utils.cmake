# 存放一些 cmake 的宏定义、函数等





# 设置编译时使用的 CPU 核心数
MACRO(cmake_utils_set_cpu_count)
  INCLUDE(ProcessorCount)
  ProcessorCount(N)
  IF(NOT N EQUAL 0)
    MATH(EXPR EX_CPU_COUNT "${N} - 2")# 使用当前核心数 - 2
    IF(NOT EX_CPU_COUNT EQUAL 0)
      MESSAGE( "Compiler use CPU Core: ${EX_CPU_COUNT}" )
      SET(CTEST_BUILD_FLAGS -j${EX_CPU_COUNT})
      SET(ctest_test_args ${ctest_test_args} PARALLEL_LEVEL ${EX_CPU_COUNT})
      SET(CMAKE_MAKE_PROGRAM "${CMAKE_MAKE_PROGRAM} -j${EX_CPU_COUNT}") 
    ENDIF()
  ENDIF()
ENDMACRO()


# 设置代码生成-运行时库
MACRO(cmake_utils_msvc_replace_md_to_mt)
  IF (MSVC)
      SET(CompilerFlags
          CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE
          CMAKE_C_FLAGS  CMAKE_C_FLAGS_DEBUG CMAKE_C_FLAGS_RELEASE )
      FOREACH(CompilerFlag ${CompilerFlags})
          STRING(REPLACE "/MD" "/MT" ${CompilerFlag} "${${CompilerFlag}}")
      ENDFOREACH()
  ENDIF(MSVC)
ENDMACRO()


# 32位下，关闭 Thread-safe Local Static Initialization
# 否则不兼容XP
MACRO(cmake_utils_msvc_close_thread_safe_init_on_32)
  IF( CMAKE_SIZEOF_VOID_P EQUAL 4 )
      SET(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /Zc:threadSafeInit-")
      SET(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} /Zc:threadSafeInit-")
      SET(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Zc:threadSafeInit-")
      SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /Zc:threadSafeInit-")
  ENDIF()
ENDMACRO()


# 设置调试信息格式
MACRO(cmake_utils_msvc_set_dbg_info_format)
  SET(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /ZI")
  SET(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} /Zi")
  SET(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /ZI")
  SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /Zi")
ENDMACRO()


# 设置 MSVC 的调试宏定义
MACRO(cmake_utils_msvc_set_debug_flags)
  IF (MSVC)
    SET(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -DNDEBUG")
    SET(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -D_DEBUG")
    SET(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -DNDEBUG")
    SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -D_DEBUG")
  ENDIF(MSVC)
ENDMACRO()


# 标准的解决方案配置
MACRO(cmake_utils_msvc_common_solution_option)
  # 设置编译时使用的 CPU 核心数
  cmake_utils_set_cpu_count()
  # 去掉 RelWithDebInfo/MinSizeRel ，仅保留 Debug/Release
  SET( CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "" FORCE )
  # 检测当前编译选项平台
  IF( CMAKE_SIZEOF_VOID_P EQUAL 8 )
      MESSAGE( "Use 64 bits compiler" )
      SET( EX_PLATFORM 64 )
      SET( EX_PLATFORM_NAME "x64" )
  ELSEIF( CMAKE_SIZEOF_VOID_P EQUAL 4 )
      MESSAGE( "Use 32 bits compiler" )
      SET( EX_PLATFORM 32 )
      SET( EX_PLATFORM_NAME "Win32" )
  ELSE()
      # 理论上不存在这种情况，直接报错
      MESSAGE( FATAL_ERROR "Not support pvoid type: ${CMAKE_SIZEOF_VOID_P}" )
  ENDIF()
  # 添加编译器选项
  IF( MSVC ) # 无论 原版MSVC 还是 LLVM (clang-cl) 都属于 MSVC
      MESSAGE( "Add MSVC compile options" )
      ADD_COMPILE_OPTIONS( /MP )        # Use multiple processors when building
	  IF(USE_LLVM)
		ADD_COMPILE_OPTIONS( /W3 /WX- )   # LLVM: Warning level 3, disable all warnings are errors
	  ELSE()
		ADD_COMPILE_OPTIONS( /W3 /WX )   # MSVC(VS compiler): Warning level 3, treat all warnings as errors
	  ENDIF()
  ELSE()
      # 对于不支持的编译器，直接报错
      MESSAGE( FATAL_ERROR "Not support compiler: ${CMAKE_CXX_COMPILER_ID}" )
      # ADD_COMPILE_OPTIONS( -W -Wall -Werror ) # All Warnings, all warnings are errors
  ENDIF()
  # 指定 c++ 标准
  set(CMAKE_CXX_STANDARD 17)
  set(CMAKE_CXX_STANDARD_REQUIRED True)
  # 强制重新生成工程文件
  SET( CMAKE_SUPPRESS_REGENERATION true ) 
  # 使用相对路径，生成的工程文件使用相对路径
  SET( CMAKE_USE_RELATIVE_PATHS true )
  # 定义工程的顶级路径，当前 CMakeLists.txt 所在的路径
  SET( PROJDIR ${CMAKE_CURRENT_SOURCE_DIR} )
  # 设置最终目标文件的输出路径，MSVC会自动在末尾加上$(Configuration)
  SET( EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/build/output/$(Platform) )
  SET( LIBRARY_OUTPUT_PATH  ${CMAKE_CURRENT_SOURCE_DIR}/build/output/$(Platform) )
  # 添加链接库目录
  LINK_DIRECTORIES( ${LIBRARY_OUTPUT_PATH} )
  # 启用文件夹管理项目
  SET_PROPERTY( GLOBAL PROPERTY USE_FOLDERS ON )
ENDMACRO()


# 标准的项目设置
MACRO(cmake_utils_msvc_common_project_option)
  # 设置本工程的包含目录
  INCLUDE_DIRECTORIES( ${CMAKE_CURRENT_SOURCE_DIR} )
  # 编译版本宏定义
  ADD_DEFINITIONS( -DUNICODE -D_UNICODE )
  # 设置 MSVC 的调试宏定义
  cmake_utils_msvc_set_debug_flags()
  # 设置代码生成-运行时库
  cmake_utils_msvc_replace_md_to_mt()
  # 设置调试信息格式
  cmake_utils_msvc_set_dbg_info_format()
  # 32位下，关闭 Thread-safe Local Static Initialization
  cmake_utils_msvc_close_thread_safe_init_on_32()
ENDMACRO()

# 标准的 static library 设置
MACRO(cmake_utils_msvc_common_lib_option)
  # 标准的项目设置
  cmake_utils_msvc_common_project_option()
  # Debug下生成文件增加后缀，对exe无效。
  SET(CMAKE_DEBUG_POSTFIX "_d")
ENDMACRO()

# 标准的 shared library 设置
MACRO(cmake_utils_msvc_common_dll_option)
  # 标准的项目设置
  cmake_utils_msvc_common_project_option()
  # Debug下生成文件增加后缀，对exe无效。
  SET(CMAKE_DEBUG_POSTFIX "D")
  # 开启PDB 和 MAP
  SET(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /DEBUG /MAP /OPT:REF /OPT:ICF")
ENDMACRO()

# 标准的 exe 设置
MACRO(cmake_utils_msvc_common_exe_option)
  # 标准的项目设置
  cmake_utils_msvc_common_project_option()
  # 开启PDB
  SET(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /DEBUG /OPT:REF /OPT:ICF")
ENDMACRO()


