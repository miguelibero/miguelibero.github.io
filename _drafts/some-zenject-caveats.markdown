---
layout: post
title:  "Some Zenject caveats and how to solve them"
date:   2015-11-29 19:58
categories: unity
---

For some time at [SocialPoint](http://www.socialpoint.es) we've been working with
Unity at work to build our 3D games, and did not have a good answer for the first
question that any experienced programmer has when first learning Unity, namely:
*This simple example looks easy to understand, but how to you create the service
structure for a complex real-world game with this?*

Until recently we were using some iteration of the [service locator pattern](http://gameprogrammingpatterns.com/service-locator.html),
but in our last project we started using dependency injection with the
[Zenject](https://github.com/modesttree/Zenject) library. It's working like a charm, mostly for the reason that
there is one clearly defined way of creating and getting a service. In our
process we found and solved some issues, in this post I will discuss them.

<!-- more -->

### Global Root Container

Zenject defines a global root container that will be loaded by any scene container.
The services defined in this global container will be maintained from one scene
to the other. This is really useful for passing backend communication services
and game state from the loading scene to the rest of game scenes.

But on the other hand we want to be able to load some local game state when
we play from a game scene, for example we want to be able to run the battle
scene by itself for testing purposes.

To do this, we created a simple `AdvancedCompositionRoot` component that
will load the defined installers into the global container instead of the scene
container.

```csharp
namespace ModestTree.Zenject
{
    public sealed class AdvancedCompositionRoot : MonoBehaviour
    {
        [SerializeField]
        public MonoInstaller[] GlobalRootInstallers = new MonoInstaller[0];

        public void Awake()
        {
            var rootContainer = GlobalCompositionRoot.Instance.Container;
            if(rootContainer != null)
            {
                rootContainer.Install(GlobalRootInstallers);
            }
        }
    }
}
```

For this to work, after adding the `AdvancedCompositionRoot` component we
will also need to make sure that it is called before the normal `CompositionRoot`
by going to the *Script Execution Order* settings and adding it with higher priority.

Finally we will load the `RealBackendInstaller` component in the loading scene
and the `FakeBackendInstaller` in the rest of scenes. In the fake backend installer
we are checking if there is already a service defined for each backend service
to not overwrite the real one.

```csharp
```

### Game Model Parts

It's really useful to have game model parts defined as Zenject services,
that way you decouple game logic a lot, for example if you only need to access
the player resources you don't need to inject the whole player model.

The easiest way of doing this is binding the parts using getters to the parent
models

```csharp
Container.Bind<IResources>().ToGetter<IPlayer>( p => p.Resources );
```

But this introduces a problems, when the player is reloaded it will create a new
resources object, and all the components that asked for it before will have
a reference to the old player resources.
