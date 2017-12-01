/* Necessary includes for device drivers */
#include <linux/init.h>
/* #include <linux/config.h> */
#include <linux/module.h>
#include <linux/kernel.h> /* printk() */
#include <linux/slab.h> /* kmalloc() */
#include <linux/fs.h> /* everything... */
#include <linux/errno.h> /* error codes */
#include <linux/types.h> /* size_t */
#include <linux/proc_fs.h>
#include <linux/fcntl.h> /* O_ACCMODE */
#include <asm/uaccess.h> /* copy_from/to_user */

#include "kmutex.h"

MODULE_LICENSE("Dual BSD/GPL");

/* Declaration of disco.c functions */
int disco_open(struct inode *inode, struct file *filp);
int disco_release(struct inode *inode, struct file *filp);
ssize_t disco_read(struct file *filp, char *buf, size_t count, loff_t *f_pos);
ssize_t disco_write(struct file *filp, const char *buf, size_t count, loff_t *f_pos);
void disco_exit(void);
int disco_init(void);

/* Structure that declares the usual file */
/* access functions */
struct file_operations disco_fops = {
  read: disco_read,
  write: disco_write,
  open: disco_open,
  release: disco_release
};

typedef struct {
  char *buffer;
  int size;
  int muerto;
  KMutex mutex;
  KCondition cond;
  int writing;
} Pipe;

typedef struct node {
    Pipe *p;
    struct node *prox;
    int listo;
} Node;

/* Declaration of the init and exit functions */
module_init(disco_init);
module_exit(disco_exit);

/*** El driver para lecturas sincronas *************************************/

#define TRUE 1
#define FALSE 0

/* Global variables of the driver */

int disco_major = 61;     /* Major number */

/* Buffer to store data */
#define MAX_SIZE 8192

static int writing;
static Node *readers_pend;
static Node *writers_pend;

/* El mutex y la condicion para disco */
static KMutex mutex;
static KCondition cond;

int disco_init(void) {
  int rc;

  /* Registering device */
  rc = register_chrdev(disco_major, "disco", &disco_fops);
  if (rc < 0) {
    printk(
      "<1>disco: cannot obtain major number %d\n", disco_major);
    return rc;
  }

  readers_pend = NULL;
  writers_pend = NULL;

  m_init(&mutex);
  c_init(&cond);

  printk("<1>Inserting disco module\n");
  return 0;
}

void disco_exit(void) {
  /* Freeing the major number */
  unregister_chrdev(disco_major, "disco");

  /* Freeing buffer disco */
  /*if (disco_buffer) {
    kfree(disco_buffer);
  }*/

  printk("<1>Removing disco module\n");
}

int disco_open(struct inode *inode, struct file *filp) {
  int rc= 0;
  Pipe *p;
  Node *nodo;

  printk("<1>Entra al open\n");
  p = (Pipe*) kmalloc(sizeof(Pipe), GFP_KERNEL);
  m_lock(&mutex);
  m_init(&p->mutex);
  c_init(&p->cond);

  if (filp->f_mode & FMODE_WRITE) {
    if (readers_pend == NULL){
      int rc;

      printk("<1>Open para el write, no hay reader\n");

      p->buffer = kmalloc(MAX_SIZE, GFP_KERNEL);
      p->size = 0;
      p->writing = 1;

      printk("<1>open request for write\n");
      /* Se debe esperar hasta que no hayan otros lectores o escritores */
      filp->private_data = p;

      nodo = (Node*) kmalloc(sizeof(Node), GFP_KERNEL);
      nodo->p = p;
      nodo->listo = FALSE;
      nodo->prox = writers_pend;
      writers_pend = nodo;

      while (!nodo->listo) {
        printk("<1>Espera al reader\n");
        if (c_wait(&cond, &mutex)) {
          printk("<1>wrte wait interrupted\n");
          c_broadcast(&cond);
          rc= -EINTR;
          goto epilog;
        }
      }
      p->size = 0;
      c_broadcast(&cond);
      printk("<1>open for write successful\n");
    }
    else {
      printk("<1>Open para el write, si hay reader\n");
      p = readers_pend->p;
      readers_pend->listo = TRUE;
      readers_pend = readers_pend->prox;
      filp->private_data = p;
      c_broadcast(&cond);
      printk("<1>Notifica al reader\n");
    }
  }
  else if (filp->f_mode & FMODE_READ) {
    if (writers_pend == NULL){
      int rc;

      printk("<1>Open para el reader, no hay writer\n");

      p->buffer = kmalloc(MAX_SIZE, GFP_KERNEL);
      p->size = 0;
      p->writing = 1;

      printk("<1>open request for read\n");
      /* Se debe esperar hasta que no hayan otros lectores o escritores */
      filp->private_data = p;

      nodo = (Node*) kmalloc(sizeof(Node), GFP_KERNEL);
      nodo->p = p;
      nodo->listo = FALSE;
      nodo->prox = readers_pend;
      readers_pend = nodo;

      while (!nodo->listo) {
        printk("<1>Espera al writer\n");
        if (c_wait(&cond, &mutex)) {
          c_broadcast(&cond);
          rc= -EINTR;
          goto epilog;
        }
      }
      p->size = 0;
      c_broadcast(&cond);
      printk("<1>open for read successful\n");
    }
    else{
      printk("<1>Open para el reader, si hay writer\n");
      p = writers_pend->p;
      writers_pend->listo = TRUE;
      writers_pend = writers_pend->prox;
      filp->private_data = p;
      c_broadcast(&cond);
      printk("<1>Notifica al writer\n");
    }
  }

epilog:
  m_unlock(&mutex);
  return rc;
}

int disco_release(struct inode *inode, struct file *filp) {
  Pipe *p;

  printk("<1>Entra al release\n");
  p = (Pipe*) filp->private_data;
  m_lock(&(p->mutex));

  if (filp->f_mode & FMODE_WRITE) {
    p->writing = FALSE;
    c_broadcast(&(p->cond));
    printk("<1>close for write successful\n");
  }
  else if (filp->f_mode & FMODE_READ) {
    if (readers_pend == NULL)
      c_broadcast(&(p->cond));
    printk("<1>close for read\n");
  }

  m_unlock(&(p->mutex));
  return 0;
}

ssize_t disco_read(struct file *filp, char *buf,
                    size_t count, loff_t *f_pos) {
  ssize_t rc;
  Pipe *p;

  printk("<1>Entra en el read\n");
  p = (Pipe*) filp->private_data;
  m_lock(&(p->mutex));

  printk("<1>size: %i\n", p->size);
  printk("<1>f_pos: %i\n", *f_pos);
  while ((p->size) <= *f_pos || !p->writing) {
    printk("<1>Espera en el read\n");
    /* si el lector esta en el final del archivo pero hay un proceso
     * escribiendo todavia en el archivo, el lector espera.
     */
    if (c_wait(&(p->cond), &(p->mutex))) {
      printk("<1>read interrupted\n");
      rc= -EINTR;
      goto epilog;
    }
  }

  if (!(p->writing)){
    return 0
  }

  printk("<1>Termina de esperar en el read\n");
  if (count > (p->size)-*f_pos) {
    count= (p->size)-*f_pos;
  }

  printk("<1>read %d bytes at %d\n", (int)count, (int)*f_pos);

  /* Transfiriendo datos hacia el espacio del usuario */
  if (copy_to_user(buf, (p->buffer)+*f_pos, count)!=0) {
    /* el valor de buf es una direccion invalida */
    rc= -EFAULT;
    goto epilog;
  }

  printk("Transferi los datos desde el espacio del usuario (reader)\n");
  *f_pos+= count;
  rc= count;
  printk("<1>Termine de leer\n");

epilog:
  m_unlock(&(p->mutex));
  return rc;
}

ssize_t disco_write( struct file *filp, const char *buf,
                      size_t count, loff_t *f_pos) {
  ssize_t rc;
  loff_t last;
  Pipe *p;

  printk("<1>Entra el write\n");
  p = (Pipe*) filp->private_data;
  m_lock(&(p->mutex));

  last= *f_pos + count;
  if (last>MAX_SIZE) {
    count -= last-MAX_SIZE;
  }
  printk("<1>write %d bytes at %d\n", (int)count, (int)*f_pos);

  /* Transfiriendo datos desde el espacio del usuario */
  if (copy_from_user((p->buffer)+*f_pos, buf, count)!=0) {
    /* el valor de buf es una direccion invalida */
    rc= -EFAULT;
    goto epilog;
  }

  printk("Transferi los datos desde el espacio del usuario (write)\n");
  *f_pos += count;
  p->size = *f_pos;
  rc= count;
  c_broadcast(&(p->cond));
  printk("<1>Termine de escribir\n");

epilog:
  m_unlock(&(p->mutex));
  return rc;
}