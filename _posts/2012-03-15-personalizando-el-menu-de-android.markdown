---
layout: post
title: Personalizando el menu de Android
updated: 15/03/2012 04:39
categories: android java
---
Estoy trabajando en una aplicación que tiene colores personalizados para el menú desplegable inferior típico de las aplicaciones Android. Según [muchas](http://stackoverflow.com/questions/2719173/change-background-color-of-android-menu) [entradas](http://stackoverflow.com/questions/2719173/change-background-color-of-android-menu) de StackOverflow esto no se puede hacer. En la documentación de android hay una [guía de estilo](http://developer.android.com/guide/practices/ui_guidelines/icon_design_menu.html) que dice que los iconos deben ser en blanco y negro. Pero si de todas formas quieres hacerlo (porque el cliente es muy pesado), así es como se hace.

<!-- more -->

Hay que sobreescribir la `LayoutInflater.Factory` justo después del evento `onCreateOptionsMenu` de la actividad.

```java
@Override
public boolean onCreateOptionsMenu(Menu menu) {
    MenuInflater inflater = getMenuInflater();
    inflater.inflate(R.menu.general, menu);
    new Handler().post(new Runnable(){
        public void run() {
            LayoutInflater inflater = getLayoutInflater();
            inflater.setFactory(new ThemedMenuLayoutInflaterFactory());
        }
    });
    return true;
}
```

La clase `ThemedMenuLayoutInflaterFactory` va a intervenir cuando detecte que el elemento a cargar es un elemento del menu.

```java
public class ThemedMenuLayoutInflaterFactory implements Factory
{
    public View onCreateView(String name, final Context context, AttributeSet attrs) {
        String className = "com.android.internal.view.menu.IconMenuItemView" ;
        if (!name.equalsIgnoreCase(className) ) {
            return null;
        }
        try {
            LayoutInflater inflater = LayoutInflater.from(context);
            final View view = inflater.createView( name, null, attrs )
            new Handler().post( new Runnable() {
                public void run () {
                    fixMenuItemView(view, context);
                }
            });
            return view;
        }catch ( Exception e ) {
        }
        return null;
    }

    protected void fixMenuItemView(View view, Context context){
        if(view instanceof TextView){
            TextView text = (TextView) view;
            text.setTextAppearance(context, R.style.themeMenuItem);
        }
    }
}
```

El primer problema que nos encontramos es que la vista es un objeto de la clase interna de android `com.android.internal.view.menu.IconMenuItemView`. Esta clase hereda de TextView, pero es que el cliente quiere modificar el icono también! (Ala venga). Para acceder al icono tendremos que echar mano de nuestra amada reflexión.

```java
protected Drawable getMenuItemIcon(View view){
    try {
        Class itemClass = Class.forName("com.android.internal.view.menu.IconMenuItemView");
        Class dataClass = Class.forName("com.android.internal.view.menu.MenuItemImpl");
        Method getData = itemClass.getDeclaredMethod("getItemData");
        Method getIcon = dataClass.getDeclaredMethod("getIcon");
        Object data = getData.invoke(view);
        Object icon = getIcon.invoke(data);
        if(icon instanceof Drawable){
            return (Drawable) icon;
        }
    } catch (Exception e) {
    }
    return null;
}

protected void fixMenuItemView(View view, Context context){
    Drawable icon = getMenuItemIcon(view);
    if(icon != null){
        int color = context.getResources().getColor(R.color.themeColorExtraForeground);
        icon.setColorFilter(color, Mode.MULTIPLY);
    }
    if(view instanceof TextView){
        TextView text = (TextView) view;
        text.setTextAppearance(context, R.style.themeMenuItem);
    }
}
```

Para terminar otro problema, que es que esto no funciona. Resulta que hay un bug en Android 2.3 que afecta al `LayoutInflater` y que hace que el inflate pete. Para solucionar el problema hay que crear un `XmlPullParser` propio y cargar el menú con él.

```java
private View hackAndroid23(String name, AttributeSet attrs, LayoutInflater inflater){
    FakeXmlPullParser parser = new FakeXmlPullParser(name, attrs, inflater);
    try {
        inflater.inflate(parser, null);
     } catch (Exception e1) {
        // "exit" ignored
    }
    return parser.getView();
}

public View onCreateView(String name, final Context context, AttributeSet attrs) {
    String className = "com.android.internal.view.menu.IconMenuItemView" ;
    if (!name.equalsIgnoreCase(className) ) {
        return null;
    }
    try {
        LayoutInflater inflater = LayoutInflater.from(context);
        final View view = inflateView(name, attrs, inflater);
        new Handler().post( new Runnable() {
            public void run () {
                fixMenuItemView(view, context);
            }
        } );
        return view;
    }catch ( Exception e ) {
    }
    return null;
}
```

He puesto la clase completa en [un gist](https://gist.github.com/2043782) para que se pueda ver entera. Esto es todo, así el cliente estará contento.
