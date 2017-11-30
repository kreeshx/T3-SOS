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
  int in, out, size;
  KMutex mutex;
  KCondition cond;
  int ready;
  int read;
} Pipe;

typedef struct nodo {
    struct file *actual_file;
    struct nodo *prox;
    int listo;
};

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

static int readers;
static int writers;
static nodo *readers_pend;
static nodo *writers_pend;

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

  writers = 0;
  readers = 0;
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

void poner_al_final(struct node *lista, struct node *file){
  if (lista == NULL){
    lista = file;
  }
  else{
    lista->prox = poner_al_final(lista->prox, file);
  }
  return lista
}

int disco_open(struct inode *inode, struct file *filp) {
  printk("<1>In disco_open\n");
  int rc= 0;

  printk("<1>Inserting disco module\n");
  m_lock(&mutex);

  /*Si es un escritor debe esperar un lector*/
  if (filp->f_mode & FMODE_WRITE) {
    Pipe *p = kmalloc(sizeof(Pipe*), GFP_KERNEL);

    /* Allocating buffer */
    p->buffer = kmalloc(MAX_SIZE, GFP_KERNEL);
    if (p->buffer==NULL) {
      disco_exit();
      return -ENOMEM;
    }
    memset(p->buffer, 0, MAX_SIZE);

    p->size = 0;
    p->ready = FALSE;

    int rc;
    printk("<1>open request for write\n");
    /* Se debe esperar hasta que no hayan otros lectores o escritores */
    writers++;
    filp->private_data = p;

    nodo *nodo_t = kmalloc(sizeof(Nodo*), GFP_KERNEL);
    nodo_t->actual_file = filp;
    nodo_t->listo = FALSE;
    nodo_t->prox = NULL;
    poner_al_final(writers, nodo);
    while (readers==0) {
      if (c_wait(&cond, &mutex)) {
      	writers--;
        c_broadcast(&cond);
        rc= -EINTR;
        goto epilog;
      }
    }
    writers--;
    p->size= 0;
    filp->private_data = p;
    c_broadcast(&cond);
    printk("<1>open for write successful\n");
  }
  /*Si es un lector debe esperar un escritor*/
  else if (filp->f_mode & FMODE_READ) {
    /* Para evitar la hambruna de los escritores, si nadie esta escribiendo
     * pero hay escritores pendientes (esperan porque readers>0), los
     * nuevos lectores deben esperar hasta que todos los lectores cierren
     * el dispositivo e ingrese un nuevo escritor.
     */
  	readers++;
    nodo *nodo_t = kmalloc(sizeof(Nodo*), GFP_KERNEL);
    nodo_t->actual_file = filp;
    nodo_t->listo = FALSE;
    nodo_t->prox = NULL;
    poner_al_final(readers, nodo);
    while (writers==0) {
      if (c_wait(&cond, &mutex)) {
        filp->private_data = writers_pend.actual_file;
      	readers--;
        c_broadcast(&cond);
        rc= -EINTR;
        goto epilog;
      }
    }
    readers--;
    filp->private_data = writers_pend.actual_file;
    c_broadcast(&cond);
    printk("<1>open for read\n");
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
    readers--;
    c_broadcast(&cond);
    printk("<1>close for read (readers remaining=%d)\n", readers);
  }

  m_unlock(&mutex);
  return 0;
}

ssize_t disco_read(struct file *filp, char *buf,
                    size_t count, loff_t *f_pos) {
  printk("<1>In disco_read\n");
  ssize_t rc;
  Pipe *p = filp->private_data;
  KMutex m = p->mutex;
  KCondition c = p->cond;
  m_lock(&m);
  while (p->size <= *f_pos && writers > 0) {
    /* si el lector esta en el final del archivo pero hay un proceso
     * escribiendo todavia en el archivo, el lector espera.
     */
    if (c_wait(&c, &m)) {
      printk("<1>read interrupted\n");
      rc= -EINTR;
      goto epilog;
    }
  }

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

  *f_pos+= count;
  rc= count;

epilog:
  m_unlock(&m);
  return rc;
}

ssize_t disco_write( struct file *filp, const char *buf,
                      size_t count, loff_t *f_pos) {
  printk("<1>In disco_write\n");
  ssize_t rc;
  loff_t last;
  Pipe *p = filp->private_data;
  KMutex m = p->mutex;
  KCondition c = p->cond;
  m_lock(&m);

  last= *f_pos + count;
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

  *f_pos += count;
  p->size= *f_pos;
  rc= count;
  c_broadcast(&c);

epilog:
  m_unlock(&m);
  return rc;
}