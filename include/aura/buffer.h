#ifndef AURA_BUFFER_H
#define AURA_BUFFER_H



/** \addtogroup retparse
 *  @{
 */


#ifndef EVBUFFER_IOVEC_IS_NATIVE_
struct evbuffer_iovec;
#endif

void aura_buffer_put_eviovec(struct aura_buffer *buf, struct evbuffer_iovec *vec, size_t length);
struct aura_buffer *aura_buffer_from_eviovec(struct aura_node *node, struct evbuffer_iovec *vec, size_t length);

void aura_buffer_release(struct aura_buffer *buf);
void aura_buffer_destroy(struct aura_buffer *buf);

struct aura_buffer *aura_buffer_request(struct aura_node *nd, int size);
size_t aura_buffer_length(struct aura_buffer *buf);

/**
 * \brief Get an unsigned 8 bit integer from aura buffer
 *
 * This function will cause a panic if attempted to read beyond
 * the buffer boundary.
 *
 * @param buf
 * @return
 */
uint8_t  aura_buffer_get_u8(struct aura_buffer *buf);

/**
 * \brief Get an unsigned 16 bit integer from aura buffer
 *
 * This function will swap endianness if needed.
 * This function will cause a panic if attempted to read beyond
 * the buffer boundary.
 *
 * @param buf
 * @return
 */
uint16_t aura_buffer_get_u16(struct aura_buffer *buf);

/**
 * \brief Get an unsigned 32 bit integer from aura buffer.
 *
 * This function will swap endianness if needed.
 * This function will cause a panic if attempted to read beyond
 * the buffer boundary.
 *
 * @param buf
 * @return
 */
uint32_t aura_buffer_get_u32(struct aura_buffer *buf);

/**
 * \brief Get an unsigned 64 bit integer from aura buffer
 *
 * This function will swap endianness if needed.
 * This function will cause a panic if attempted to read beyond
 * the buffer boundary.
 *
 * @param buf
 * @return
 */
uint64_t aura_buffer_get_u64(struct aura_buffer *buf);

/**
 * \brief Get a signed 8 bit integer from aura buffer.
 *
 * This function will cause a panic if attempted to read beyond
 * the buffer boundary.
 * @param buf
 * @return
 */
int8_t  aura_buffer_get_s8(struct aura_buffer *buf);

/**
 * \brief Get a signed 16 bit integer from aura buffer
 *
 * This function will swap endianness if needed.
 * This function will cause a panic if attempted to read beyond
 * the buffer boundary.
 *
 * @param buf
 * @return
 */
int16_t aura_buffer_get_s16(struct aura_buffer *buf);

/**
 * \brief Get a signed 32 bit integer from aura buffer
 *
 * This function will swap endianness if needed.
 * This function will cause a panic if attempted to read beyound
 * the buffer boundary.
 *
 * @param buf
 * @return
 */
int32_t aura_buffer_get_s32(struct aura_buffer *buf);

/**
 * \brief Get a signed 64 bit integer from aura buffer
 *
 * This function will swap endianness if needed.
 * This function will cause a panic if attempted to read beyond
 * the buffer boundary.
 *
 *
 * @param buf
 * @return
 */
int64_t aura_buffer_get_s64(struct aura_buffer *buf);

/**
 * \brief Put an unsigned 8 bit integer to aura buffer
 *
 * This function will cause a panic if attempted to read beyond
 * the buffer boundary.
 *
 * @param buf
 * @param value
 * @return
 */
void  aura_buffer_put_u8(struct aura_buffer *buf, uint8_t value);

/**
 * \brief Put an unsigned 16 bit integer to aura buffer
 *
 * This function will swap endianness if needed.
 * This function will cause a panic if attempted to read beyond
 * the buffer boundary.
 *
 * @param buf
 * @param value
 * @return
 */
void aura_buffer_put_u16(struct aura_buffer *buf, uint16_t value);

/**
 * \brief Put an unsigned 32 bit integer to aura buffer.
 *
 * This function will swap endianness if needed.
 * This function will cause a panic if attempted to read beyond
 * the buffer boundary.
 *
 * @param buf
 * @param value
 * @return
 */
void aura_buffer_put_u32(struct aura_buffer *buf, uint32_t value);

/**
 * \brief Put an unsigned 64 bit integer to aura buffer
 *
 * This function will swap endianness if needed.
 * This function will cause a panic if attempted to read beyond
 * the buffer boundary.
 *
 * @param buf
 * @param value
 * @return
 */
void aura_buffer_put_u64(struct aura_buffer *buf, uint64_t value);

/**
 * \brief Put a signed 8 bit integer to aura buffer.
 *
 * This function will cause a panic if attempted to read beyond
 * the buffer boundary.
 * @param buf
 * @param value
 * @return
 */
void  aura_buffer_put_s8(struct aura_buffer *buf, int8_t value);

/**
 * \brief Put a signed 16 bit integer to aura buffer
 *
 * This function will swap endianness if needed.
 * This function will cause a panic if attempted to read beyond
 * the buffer boundary.
 *
 * @param buf
 * @param value
 * @return
 */
void aura_buffer_put_s16(struct aura_buffer *buf, int16_t value);

/**
 * \brief Put a signed 32 bit integer to aura buffer
 *
 * This function will swap endianness if needed.
 * This function will cause a panic if attempted to read beyound
 * the buffer boundary.
 *
 * @param buf
 * @param value
 * @return
 */
void aura_buffer_put_s32(struct aura_buffer *buf, int32_t value);

/**
 * \brief Put a signed 64 bit integer to aura buffer
 *
 * This function will swap endianness if needed.
 * This function will cause a panic if attempted to read beyond
 * the buffer boundary.
 *
 *
 * @param buf
 * @param value
 * @return
 */
void aura_buffer_put_s64(struct aura_buffer *buf, int64_t value);


/**
 * \brief Get a pointer to the binary data block within buffer and advance
 * internal pointer by len bytes. The data in the buffer is managed internally and should
 * not be freed by the caller.
 *
 * This function will cause a panic if attempted to read beyond
 * the buffer boundary.
 *
 * @param buf aura buffer
 * @param len data length
 */
const void *aura_buffer_get_bin(struct aura_buffer *buf, int len);


/**
 * \brief Copy data of len bytes to aura buffer from a buffer pointed by data
 *
 * This function will advance internal aura_buffer pointer by len bytes.
 *
 * This function will cause a panic if attempted to read beyond
 * the buffer boundary.
 *
 * @param buf aura buffer
 * @param data data buffer
 * @param len data length
 */
void aura_buffer_put_bin(struct aura_buffer *buf, const void *data, int len);


/**
 * \brief Retrieve aura_buffer pointer from within the buffer and advance
 * internal pointer by it's size.
 *
 * The underlying transport must support passing aura_buffer as arguments
 *
 * This function will cause a panic if attempted to read beyond
 * the buffer boundary.
 *
 * @param buf aura buffer
 * @param len data length
 */
struct aura_buffer *aura_buffer_get_buf(struct aura_buffer *buf);

/**
 * Retrieve transport-specific pointer from aura buffer.
 * Handling of this data-type is transport specific.
 *
 * @param buf
 */
const void *aura_buffer_get_ptr(struct aura_buffer *buf);

void *aura_buffer_payload_ptr(struct aura_buffer *buf);
size_t aura_buffer_payload_length(struct aura_buffer *buf);
void aura_buffer_put_buf(struct aura_buffer *to, struct aura_buffer *to_put);

void aura_buffer_rewind(struct aura_buffer *buf);
/**
 * @}
 */





#endif /* end of include guard: AURA_BUFFER_H */
