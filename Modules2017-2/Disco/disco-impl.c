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

static char *disco_buffer;
static ssize_t curr_size;
static int readers;
static int writing;
static int pend_open_write;

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

  readers= 0;
  writing= FALSE;
  pend_open_write= 0;
  curr_size= 0;
  m_init(&mutex);
  c_init(&cond);

  /* Allocating disco_buffer */
  disco_buffer = kmalloc(MAX_SIZE, GFP_KERNEL);
  if (disco_buffer==NULL) {
    disco_exit();
    return -ENOMEM;
  }
  memset(disco_buffer, 0, MAX_SIZE);

  printk("<1>Inserting disco module\n");
  return 0;
}

void disco_exit(void) {
  /* Freeing the major number */
  unregister_chrdev(disco_major, "disco");

  /* Freeing buffer disco */
  if (disco_buffer) {
    kfree(disco_buffer);
  }

  printk("<1>Removing disco module\n");
}

int disco_open(struct inode *inode, struct file *filp) {
  int rc= 0;
  m_lock(&mutex);

  if (filp->f_mode & FMODE_WRITE) {
    int rc;
    printk("<1>open request for write\n");
    /* Se debe esperar hasta que no hayan otros lectores o escritores */
    pend_open_write++;
    while (writing || readers>0) {
      if (c_wait(&cond, &mutex)) {
        pend_open_write--;
        c_broadcast(&cond);
        rc= -EINTR;
        goto epilog;
      }
    }
    writing= TRUE;
    pend_open_write--;
    curr_size= 0;
    c_broadcast(&cond);
    printk("<1>open for write successful\n");
  }
  else if (filp->f_mode & FMODE_READ) {
    /* Para evitar la hambruna de los escritores, si nadie esta escribiendo
     * pero hay escritores pendientes (esperan porque readers>0), los
     * nuevos lectores deben esperar hasta que todos los lectores cierren
     * el dispositivo e ingrese un nuevo escritor.
     */
    while (!writing && pend_open_write>0) {
      if (c_wait(&cond, &mutex)) {
        rc= -EINTR;
        goto epilog;
      }
    }
    readers++;
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
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

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
        else { /* no hay mujeres esperando
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
  m_lock(&mutex);

  if (filp->f_mode & FMODE_WRITE) {
    writing= FALSE;
    c_broadcast(&cond);
    printk("<1>close for write successful\n");
  }
  else if (filp->f_mode & FMODE_READ) {
    readers--;
    if (readers==0)
      c_broadcast(&cond);
    printk("<1>close for read (readers remaining=%d)\n", readers);
  }

  m_unlock(&mutex);
  return 0;
}

ssize_t disco_read(struct file *filp, char *buf,
                    size_t count, loff_t *f_pos) {
  ssize_t rc;
  m_lock(&mutex);

  while (curr_size <= *f_pos && writing) {
    /* si el lector esta en el final del archivo pero hay un proceso
     * escribiendo todavia en el archivo, el lector espera.
     */
    if (c_wait(&cond, &mutex)) {
      printk("<1>read interrupted\n");
      rc= -EINTR;
      goto epilog;
    }
  }

  if (count > curr_size-*f_pos) {
    count= curr_size-*f_pos;
  }

  printk("<1>read %d bytes at %d\n", (int)count, (int)*f_pos);

  /* Transfiriendo datos hacia el espacio del usuario */
  if (copy_to_user(buf, disco_buffer+*f_pos, count)!=0) {
    /* el valor de buf es una direccion invalida */
    rc= -EFAULT;
    goto epilog;
  }

  *f_pos+= count;
  rc= count;

epilog:
  m_unlock(&mutex);
  return rc;
}

ssize_t disco_write( struct file *filp, const char *buf,
                      size_t count, loff_t *f_pos) {
  ssize_t rc;
  loff_t last;

  m_lock(&mutex);

  last= *f_pos + count;
  if (last>MAX_SIZE) {
    count -= last-MAX_SIZE;
  }
  printk("<1>write %d bytes at %d\n", (int)count, (int)*f_pos);

  /* Transfiriendo datos desde el espacio del usuario */
  if (copy_from_user(disco_buffer+*f_pos, buf, count)!=0) {
    /* el valor de buf es una direccion invalida */
    rc= -EFAULT;
    goto epilog;
  }

  *f_pos += count;
  curr_size= *f_pos;
  rc= count;
  c_broadcast(&cond);

epilog:
  m_unlock(&mutex);
  return rc;
}