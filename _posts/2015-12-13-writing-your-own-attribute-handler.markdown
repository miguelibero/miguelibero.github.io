---
layout: post
title: "Writing your own member attribute handler in Unity"
date: 2015-12-13
categories: unity csharp
---

Attributes are the main way C# implements annotations. They are very powerful
tools to simplify adding Behaviours. I'm going to explain a simple way of
implementing some functionality attached to member attributes, mainly
focused on Unity3D.

<!-- more -->

When defining an attribute attached to a property or field, we need
to create a class that inherits from `System.Attribute`. Parameters
passed when using the attribute can be named using properties or
unnamed using constructor arguments.

For example purposes we are going to write an attribute that will
be used to find a child game object with a given name.

```csharp
public class FindChildAttribute : System.Attribute
{
  public string Name { get; private set; }

  public FindChildAttribute(string name)
  {
    Name = name;
  }
}
```

This attribute can directly be used as an annotation in the following form.

```csharp
using UnityEngine;

public class ExampleBehaviour : MonoBehaviour
{
  [FindChild("Child")]
  GameObject _child;

  [FindChild("Child")]
  Transform _childTransform;
}
```

This code would compile, but it wouldn't do anything. Most people assume
that attributes add some functionality to the annotated code parts, but in
reality they just add metadata, the functionality needs to be added elsewhere
by reading the metadata.

First of all, we will define the interface
`IAttributeApplier` that will get called when we find an annotated
member, first we will ask if the applier `Supports` that particular member type,
then we will call `Apply` to get the new member value.

```csharp
using System;

public interface IMemberAttributeApplier<A> : IDisposable, ICloneable where A : Attribute
{
    bool Supports(Type memberType, A attr);

    object Apply(object obj, Type memberType, A attr);
}
```

The second part of the system is a manager class that contains a list
of applier prototypes. This class has a helper method that checks for all
the annotated members of a given object.

To generalize we first define an interface `IObjectApplier` that will be used
in the future to not depend on the exact member attributes implementation.

```csharp
using System;

interface IObjectApplier : IDisposable
{
    void Apply(object Behaviour);
}
```

```csharp
using System;
using System.Reflection;
using System.Collections.Generic;

public class MemberAttributesApplier<A> : IObjectApplier where A : Attribute
{
    IList<IMemberAttributeApplier<A>> _prototypes;
    IDictionary<A, IMemberAttributeApplier<A>> _appliers;

    public MemberAttributesApplier(List<IMemberAttributeApplier<A>> prototypes = null)
    {
        _appliers = new Dictionary<A, IMemberAttributeApplier<A>>();
        if (prototypes == null)
        {
            prototypes = new List<IMemberAttributeApplier<A>>();
        }
        _prototypes = prototypes;
    }

    public virtual void Dispose()
    {
        foreach (var proto in _prototypes)
        {
            proto.Dispose();
        }
        _prototypes.Clear();
        foreach (var kvp in _appliers)
        {
            kvp.Value.Dispose();
        }
        _appliers.Clear();
    }

    public void AddApplier(IMemberAttributeApplier<A> applier)
    {
        if (applier == null)
        {
            throw new ArgumentNullException("applier");
        }
        if (!_prototypes.Contains(applier))
        {
            _prototypes.Add(applier);
        }
    }

    object Apply(object obj, Type memberType, object member, A attr)
    {
        IMemberAttributeApplier<A> applier = null;
        if (!_appliers.TryGetValue(attr, out applier))
        {
            foreach (var proto in _prototypes)
            {
                if (proto.Supports(memberType, attr))
                {
                    applier = (IMemberAttributeApplier<A>)proto.Clone();
                    _appliers.Add(attr, applier);
                    break;
                }
            }
        }
        if (applier != null)
        {
            return applier.Apply(obj, memberType, attr);
        }
        throw new InvalidOperationException(
            string.Format("Could not find any way to apply object of type '{0}'.", memberType.FullName));
    }

    const BindingFlags MemberBindingFlags = BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Instance | BindingFlags.DeclaredOnly;

    public void Apply(object obj)
    {
        if (obj == null)
        {
            return;
        }
        var type = obj.GetType();
        foreach (var prop in type.GetProperties(MemberBindingFlags))
        {
            foreach (var attrObj in prop.GetCustomAttributes(typeof(A), true))
            {
                var attr = (A)attrObj;
                var val = prop.GetValue(obj, null);
                val = Apply(obj, prop.PropertyType, val, attr);
                prop.SetValue(obj, val, null);
            }
        }
        foreach (var field in type.GetFields(MemberBindingFlags))
        {
            foreach (var attrObj in field.GetCustomAttributes(typeof(A), true))
            {
                var attr = (A)attrObj;
                var val = field.GetValue(obj);
                val = Apply(obj, field.FieldType, val, attr);
                field.SetValue(obj, val);
            }
        }
    }
}
```

Now we have a system in place to modify the members of an object depending on the
annotated attributes. This could be directly used in any C# project, let's talk about
how to use it in a Unity project.

Where is the best way to do this with all `MonoBehaviour`
objects in a scene? In Java the typical way of doing this is the class loader,
but in C# Unity there is no such thing. Typically `MonoBehaviour` objects are created on scene load or by
code when calling `Instantiate` or `AddComponent`. The first event can be detected
with the `OnLevelWasLoaded` message, and the second we can fix with extension methods.

```csharp
using UnityEngine;
using System;
using System.Collections.Generic;

class SceneObserver : MonoBehaviour
{
    HashSet<IObjectApplier> _appliers = new HashSet<IObjectApplier>();

    public void AddApplier(IObjectApplier applier)
    {
        _appliers.Add(applier);
    }

    public void Apply(UnityEngine.Object obj)
    {
        foreach(var applier in _appliers)
        {
            applier.Apply(obj);
        }
    }

    public void Apply(GameObject go)
    {
        foreach(var comp in go.GetComponents<Component>())
        {
            Apply(comp);
        }
    }

    public void Apply()
    {
        foreach (var trans in UnityEngine.Object.FindObjectsOfType<Transform>())
        {
            if(trans.parent == null)
            {
                Apply(trans.gameObject);
            }
        }
    }

    static SceneObserver _instance;
    public static SceneObserver Instance
    {
        get
        {
            if(_instance == null)
            {
                _instance = UnityEngine.Object.FindObjectOfType<SceneObserver>();
            }
            if(_instance == null)
            {
                var go = new GameObject("SceneObserver");
               _instance = go.AddComponent<SceneObserver>();
            }
            return _instance;
        }
    }

    void Awake()
    {
        if (_instance != null && _instance != this)
        {
            throw new InvalidOperationException("Already a SceneObserver present");
        }
        _instance = this;
        DontDestroyOnLoad(transform.gameObject);
    }

    void Start()
    {
        Apply();
    }

    void OnLevelWasLoaded(int level)
    {
        Apply();
    }
}
```

Notice that the `SceneObserver` is setup as a singleton object. This is done to
simplify access to it and could be removed if we were using dependency injection.

Finding recursively all `MonoBehaviour` objects in a scene can be really expensive,
so I recommend only doing this for active root game objects.

```csharp
using UnityEngine;

static class UnityAnnotationExtensions
{
    public static GameObject ApplyAnnotations(this GameObject obj)
    {
        var sceneObserver = SceneObserver.Instance;
        if(sceneObserver != null)
        {
            sceneObserver.Apply(obj);
        }
        return obj;
    }

    public static T ApplyAnnotations<T>(this T obj) where T : Object
    {
        var sceneObserver = SceneObserver.Instance;
        if(sceneObserver != null)
        {
            sceneObserver.Apply(obj);
        }
        return obj;
    }
}
```

With this code we just have to remember to call `ApplyAnnotations` after instantiating
a game object or adding a component for the annotations to take effect.

To finish this, let's talk about how to implement the `FindChild` annotation.
We will create two applier classes, one for game objects and one for components.

```csharp
using System;
using UnityEngine;

public class FindChildGameObjectApplier : IMemberAttributeApplier<FindChildAttribute>
{
    public object Clone()
    {
        return new FindChildGameObjectApplier();
    }

    public void Dispose()
    {
    }

    public bool Supports(Type memberType, FindChildAttribute attr)
    {
        return memberType == typeof(GameObject);
    }

    public object Apply(object obj, Type memberType, FindChildAttribute attr)
    {
        var parent = obj as Component;
        if(parent == null)
        {
            return null;
        }
        var child = parent.transform.FindChild(attr.Name);
        if(child == null)
        {
            return null;
        }
        return child.gameObject;
    }
}
```

```csharp
using System;
using UnityEngine;

public class FindChildComponentApplier : IMemberAttributeApplier<FindChildAttribute>
{
    public object Clone()
    {
        return new FindChildComponentApplier();
    }

    public void Dispose()
    {
    }

    public bool Supports(Type memberType, FindChildAttribute attr)
    {
        return memberType.IsSubclassOf(typeof(Component));
    }

    public object Apply(object obj, Type memberType, FindChildAttribute attr)
    {
        var parent = obj as Component;
        if(parent == null)
        {
            return null;
        }
        var child = parent.transform;
        if(!string.IsNullOrEmpty(attr.Name))
        {
            child = parent.transform.FindChild(attr.Name);
        }
        if(child == null)
        {
            return null;
        }
        return child.GetComponentInChildren(memberType);
    }
}
```

Both appliers are very similar, they are just separated since we need to use
`FindChild` to find by name and `GetComponentInChildren` to find by component.

Finally, so that these appliers are used, we will need to setup our `SceneObserver`
in some place (typically where the game services are loaded and configured).

```csharp
var observer = SceneObserver.Instance;
var findChild = new MemberAttributesApplier<FindChildAttribute>();
findChild.AddApplier(new FindChildGameObjectApplier());
findChild.AddApplier(new FindChildComponentApplier());
observer.AddApplier(findChild);
```

This should always happen before the annotated `MonoBehaviour` objects are started,
so maybe you will need to use the script execution order settings.
