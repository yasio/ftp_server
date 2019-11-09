set (FTP_SERVER_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR})

set (FTP_SERVER_SRC ${FTP_SERVER_SRC_DIR}/main.cpp
    ${FTP_SERVER_SRC_DIR}/ftp_session.cpp
    ${FTP_SERVER_SRC_DIR}/ftp_server.cpp
)

set (FTP_SERVER_INC_DIR ${FTP_SERVER_SRC_DIR}/../../)

include_directories ("${FTP_SERVER_SRC_DIR}")
include_directories ("${FTP_SERVER_INC_DIR}")

add_executable (ftp_server ${FTP_SERVER_SRC}) 

if (WIN32)
    set (FTP_SERVER_LDLIBS yasio)
else ()
    set (FTP_SERVER_LDLIBS yasio pthread)
endif()

target_link_libraries (ftp_server ${FTP_SERVER_LDLIBS})
