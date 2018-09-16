#ifndef BUFFERS_H
#define BUFFERS_H

void sip_buffers_init(void);
int sip_buffers_allocate(void);
void sip_buffers_free(void);

struct t_gui_buffer* sip_buffers_get(const char* contact_in, size_t len);
void sip_buffers_delete(const char* contact);

#endif // BUFFERS_H
