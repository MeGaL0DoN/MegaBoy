#define ST_WRITE(var) st.write(reinterpret_cast<const char*>(&var), sizeof(var))
#define ST_WRITE_ARR(var) st.write(reinterpret_cast<const char*>(var.data()), sizeof(var))

#define ST_READ(var) st.read(reinterpret_cast<char*>(&var), sizeof(var))
#define ST_READ_ARR(var) st.read(reinterpret_cast<char*>(var.data()), sizeof(var))