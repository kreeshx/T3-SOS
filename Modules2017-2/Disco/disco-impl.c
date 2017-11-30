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

/* CODIGO DE disco_IMPL.C*/

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
  KMutex mutex;
  KCondition cond;
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

static Node *readers_pend;
static Node *writers_pend;

/* El mutex y la condicion para disco */
static KMutex mutex;
static KCondition cond;

int disco_init(void) {
  printk("<1>In disco_init\n");
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
  printk("<1>Out disco_init\n");
  return 0;
}

void disco_exit(void) {
  printk("<1>In disco_exit\n");
  /* Freeing the major number */
  unregister_chrdev(disco_major, "disco");

  /* Freeing buffer disco
  if (buffer) {
    kfree(buffer);
  }
  */

  printk("<1>Removing disco module\n");
}


int disco_open(struct inode *inode, struct file *filp) {
  printk("<1>In disco_open\n");
  int rc= 0;
  Pipe *p;
  Node *nodo;
  KMutex m;
  KCondition c;

  printk("<1>Inserting disco module\n");
  m_lock(&mutex);

  /*Si es un escritor debe esperar un lector*/
  if (filp->f_mode & FMODE_WRITE) {
    printk("<1>In disco_open2\n");
    if (readers_pend == NULL){
      printk("<1>In disco_open write primer if\n");
      p = (Pipe*) kmalloc(sizeof(Pipe*), GFP_KERNEL);
      printk("<1>In disco_open write primer if2\n");
      m_init(&m);
      printk("<1>In disco_open write primer if3\n");
      c_init(&c);
      printk("<1>In disco_open write primer if4\n");
      p->mutex = m;
      printk("<1>In disco_open write primer if5\n");
      p->cond = c;
      printk("<1>In disco_open write primer if6\n"); 

      /* Allocating buffer */
      p->buffer = kmalloc(MAX_SIZE, GFP_KERNEL);
      printk("<1>In disco_open write primer if7\n");
      p->size = 0;
      printk("<1>In disco_open write primer if8\n");

      printk("<1>open request for write\n");
      /* Se debe esperar hasta que no hayan otros lectores o escritores */
      filp->private_data = p;
      printk("<1>In disco_open write primer if9\n");

      nodo = kmalloc(sizeof(Node*), GFP_KERNEL);
      printk("<1>In disco_open write primer if10\n");
      nodo->p = p;
      printk("<1>In disco_open write primer if11\n");
      nodo->listo = FALSE;
      printk("<1>In disco_open write primer if12\n");
      nodo->prox = writers_pend;
      printk("<1>In disco_open write primer if13\n");
      writers_pend = nodo;
      printk("<1>In disco_open write primer if14\n");
      while (!nodo->listo) {
        if (c_wait(&cond, &mutex)) {
          c_broadcast(&cond);
          rc= -EINTR;
          goto epilog;
        }
      }
      printk("<1>In disco_open write primer if15\n");
      p->size= 0;
      printk("<1>In disco_open write primer if16\n");
      c_broadcast(&cond);
      printk("<1>open for write successful\n");
    }
    else {
      printk("<1>In disco_open write segundo if\n");
      Pipe *p = readers_pend->p;
      printk("<1>In disco_open write segundo if2\n");
      readers_pend->listo = TRUE;
      printk("<1>In disco_open write segundo if3\n");
      readers_pend = readers_pend->prox;
      printk("<1>In disco_open write segundo if4\n");
      filp->private_data = p;
      printk("<1>In disco_open write segundo if5\n");
      c_broadcast(&cond);
      printk("<1>In disco_open write segundo if6\n");
      //saca el valor de la lista readers
      //valor que saque de readers lo pongo como ready
      //actualizo readers
      //guardo el pipe en el filp
      //broadcast

    }
  }
  /*Si es un lector debe esperar un escritor*/
  else if (filp->f_mode & FMODE_READ) {
    printk("<1>In disco_open3\n");
    if (writers_pend == NULL){
      printk("<1>In disco_open read primer if\n");
      p = kmalloc(sizeof(Pipe*), GFP_KERNEL);
      printk("<1>In disco_open read primer if2\n");
      m_init(&m);
      printk("<1>In disco_open read primer if3\n");
      c_init(&c);
      printk("<1>In disco_open read primer if4\n");
      p->mutex = m;
      printk("<1>In disco_open read primer if5\n");
      p->cond = c; 
      printk("<1>In disco_open read primer if6\n");

      /* Allocating buffer */
      p->buffer = kmalloc(MAX_SIZE, GFP_KERNEL);
      printk("<1>In disco_open read primer if7\n");
      p->size = 0;
      printk("<1>In disco_open read primer if8\n");

      printk("<1>open request for read\n");
      /* Se debe esperar hasta que no hayan otros lectores o escritores */
      filp->private_data = p;
      printk("<1>In disco_open read primer if9\n");

      Node *nodo = kmalloc(sizeof(Node*), GFP_KERNEL);
      printk("<1>In disco_open read primer if10\n");
      nodo->p = p;
      printk("<1>In disco_open read primer if11\n");
      nodo->listo = FALSE;
      printk("<1>In disco_open read primer if12\n");
      nodo->prox = readers_pend;
      printk("<1>In disco_open read primer if13\n");
      readers_pend = nodo;
      printk("<1>In disco_open read primer if14\n");
      while (!nodo->listo) {
        if (c_wait(&cond, &mutex)) {
          c_broadcast(&cond);
          rc= -EINTR;
          goto epilog;
        }
      }
      printk("<1>In disco_open read primer if15\n");
      p->size= 0;
      printk("<1>In disco_open read primer if16\n");
      c_broadcast(&cond);
      printk("<1>open for read successful\n");
    }
    else {
      printk("<1>In disco_open read segundo if\n");
      Pipe *p = writers_pend->p;
      printk("<1>In disco_open read segundo if2\n");
      writers_pend->listo = TRUE;
      printk("<1>In disco_open read segundo if3\n");
      writers_pend = writers_pend->prox;
      printk("<1>In disco_open read segundo if4\n");
      filp->private_data = p;
      printk("<1>In disco_open read segundo if5\n");
      c_broadcast(&cond);
      //saca el valor de la lista readers
      //valor que saque de readers lo pongo como ready
      //actualizo readers
      //guardo el pipe en el filp
      //broadcast

    }
  }

epilog:
  m_unlock(&mutex);
  return rc;
}


/*
#define FALSE 0
#define TRUE 1

typedef struct nodo {
    char *nombre, *pareja;
    int listo;
    struct nodo *prox;
} Nodo;

Nodo *hombres = NULL;
Nodo *mujeres = NULL;

void emparejar(char sexo, char *nombre, char *pareja)
{
    Nodo nodo;
    nodo.nombre = nombre;
    nodo.pareja = pareja;
    nodo.listo = FALSE;

    pthread_mutex_lock(&mutex);
    if (sexo == 'm') { // caso hombre
        if (mujeres != NULL) { // hay una mujer esperando?
            strcpy(pareja, mujeres->nombre);
            strcpy(mujeres->pareja, nombre);
            mujeres->listo = TRUE;   // desbloqueamos a la mujer
            mujeres = mujeres->prox; // y la sacamos de la lista
        }
        else { // no hay mujeres esperando
            nodo.prox = hombres; // se agrega a la lista de hombres
            hombres = &nodo;     // en espera
            while (!nodo.listo) // esperar una mujer cambiara nodo.listo
                pthread_cond_wait(&cond, &mutex);
        }
    }
    else { //caso mujer
        if (hombres != NULL) {
            strcpy(pareja, hombres->nombre);
            strcpy(hombres->pareja, nombre);
            hombres->listo= TRUE;
            hombres= hombres->next;
        }
        else {
            nodo.next = mujeres;
            mujeres = &nodo;
            while (!nodo.listo)
                pthread_cond_wait(&cond, &mutex);
        }
    }
    phtread_cond_broadcast(&cond);
    pthread_mutex_unlock(&mutex);
}
*/
/*cuando cierro el write tambien tengo ue cerrar el read*/
int disco_release(struct inode *inode, struct file *filp) {
  printk("<1>In disco_release\n");
  m_lock(&mutex);

  if (filp->f_mode & FMODE_WRITE) {
    c_broadcast(&cond);
    printk("<1>close for write successful\n");
  }
  else if (filp->f_mode & FMODE_READ) {
    c_broadcast(&cond);
    printk("<1>close for read\n");
  }

  m_unlock(&mutex);
  return 0;
}

ssize_t disco_read(struct file *filp, char *buf,
                    size_t count, loff_t *f_pos) {
  printk("<1>In disco_read\n");
  ssize_t rc;
  printk("<1>In disco_read2\n");
  Pipe *p;
  KMutex m;
  KCondition c;
  printk("<1>In disco_read3\n");
  p = filp->private_data;
  printk("<1>In disco_read4\n");
  m = p->mutex;
  c = p->cond;
  printk("<1>In disco_read5\n");
  m_lock(&m);
  printk("<1>In disco_read6\n");
  printk("<1>               size:%d, pos: %d\n", (int)(p->size), (int)(*f_pos));
  while (p->size <= *f_pos) {
    printk("<1>               size:%d, pos: %d\n", (int)(p->size), (int)(*f_pos));
    /* si el lector esta en el final del archivo pero hay un proceso
     * escribiendo todavia en el archivo, el lector espera.
     */
    printk("<1>In disco_read while\n");
    if (c_wait(&c, &m)) {
      printk("<1>read interrupted\n");
      rc= -EINTR;
      goto epilog;
    }
    printk("<1>In disco_read while 2\n");
  }
  printk("<1>In disco_read7\n");

  if (count > p->size-*f_pos) {
    count= p->size-*f_pos;
  }
  printk("<1>read %d bytes at %d\n", (int)count, (int)*f_pos);

  /* Transfiriendo datos hacia el espacio del usuario */
  if (copy_to_user(buf, p->buffer+*f_pos, count)!=0) {
    /* el valor de buf es una direccion invalida */
    rc= -EFAULT;
    goto epilog;
  }
  printk("<1>In disco_read8\n");
  *f_pos+= count;
  printk("<1>In disco_read9\n");
  rc= count;
  printk("<1>In disco_read10\n");

epilog:
  m_unlock(&m);
  return rc;
}

ssize_t disco_write( struct file *filp, const char *buf,
                      size_t count, loff_t *f_pos) {
  printk("<1>In disco_write\n");
  ssize_t rc;
  printk("<1>In disco_write2\n");
  loff_t last;
  printk("<1>In disco_write3\n");
  Pipe *p;
  KMutex m;
  KCondition c;
  printk("<1>In disco_write4\n");
  p = filp->private_data;
  printk("<1>In disco_write5\n");
  m = p->mutex;
  c = p->cond;
  printk("<1>In disco_write6\n");
  m_lock(&m);
  printk("<1>In disco_write7\n");

  last= *f_pos + count;
  printk("<1>In disco_write8\n");
  if (last>MAX_SIZE) {
    count -= last-MAX_SIZE;
  }
  printk("<1>write %d bytes at %d\n", (int)count, (int)*f_pos);

  /* Transfiriendo datos desde el espacio del usuario */
  if (copy_from_user(p->buffer+*f_pos, buf, count)!=0) {
    /* el valor de buf es una direccion invalida */
    rc= -EFAULT;
    goto epilog;
  }
  printk("<1>In disco_write9\n");
  *f_pos += count;
  printk("<1>In disco_write10\n");
  p->size= *f_pos;
  printk("<1>In disco_write11\n");
  rc= count;
  printk("<1>In disco_write12\n");
  c_broadcast(&c);
  printk("<1>In disco_write13\n");

epilog:
  m_unlock(&m);
  return rc;
}